// SPDX-License-Identifier: GPL-2.0
/**
 * dwc3-exynos-otg.c - DesignWare Exynos USB3 DRD Controller OTG
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Authors: Ido Shayevitz <idos@codeaurora.org>
 *	    Anton Tikhomirov <av.tikhomirov@samsung.com>
 *	    Minho Lee <minho55.lee@samsung.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/suspend.h>
#if defined(CONFIG_OTG_DEFAULT)
#include <linux/usb/typec.h>
#endif

#include "core.h"
#include "core-exynos.h"
#include "exynos-otg.h"
#include "io.h"
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
#include "../../base/base.h"
#if defined(CONFIG_OTG_CDP_SUPPORT)
#include <linux/battery/sec_battery_common.h>
#endif
#if IS_ENABLED(CONFIG_USB_EXYNOS_TPMON_MODULE)
#include "exynos_usb_tpmon.h"
#endif
#include <linux/usb/composite.h>
#include <linux/reboot.h>
#include "dwc3-exynos.h"

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
#include <linux/usb/f_ss_mon_gadget.h>
#endif

#define OTG_NO_CONNECT		0
#define OTG_CONNECT_ONLY	1
#define OTG_DEVICE_CONNECT	2
#define LINK_DEBUG_L		(0x0C)
#define LINK_DEBUG_H		(0x10)
#define BUS_ACTIVITY_CHECK	(0x3F << 16)
#define READ_TRANS_OFFSET	10

#ifndef CONFIG_SND_EXYNOS_USB_AUDIO
int otg_connection;
#endif

int is_otg_only;
EXPORT_SYMBOL_GPL(is_otg_only);
struct dwc3_exynos *g_dwc3_exynos;

/* -------------------------------------------------------------------------- */
#if defined(CONFIG_OTG_DEFAULT)
struct intf_typec {
	/* struct mutex lock; */ /* device lock */
	struct device *dev;
	struct typec_port *port;
	struct typec_capability cap;
	struct typec_partner *partner;
};
#endif

static int usb_reboot_noti(struct notifier_block *nb, unsigned long event, void *buf);
static struct notifier_block usb_reboot_notifier = {
	.notifier_call = usb_reboot_noti,
};

static int dwc3_otg_statemachine(struct otg_fsm *fsm)
{
	struct usb_otg *otg = fsm->otg;
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	enum usb_otg_state prev_state = otg->state;
	int ret = 0;

	if (dotg->fsm_reset) {
		if (otg->state == OTG_STATE_A_HOST) {
			otg_drv_vbus(fsm, 0);
			otg_start_host(fsm, 0);
		} else if (otg->state == OTG_STATE_B_PERIPHERAL) {
			otg_start_gadget(fsm, 0);
		}

		otg->state = OTG_STATE_UNDEFINED;
		goto exit;
	}

	switch (otg->state) {
	case OTG_STATE_UNDEFINED:
		if (fsm->id)
			otg->state = OTG_STATE_B_IDLE;
		else
			otg->state = OTG_STATE_A_IDLE;
		break;
	case OTG_STATE_B_IDLE:
		if (!fsm->id) {
			otg->state = OTG_STATE_A_IDLE;
		} else if (fsm->b_sess_vld) {
			ret = otg_start_gadget(fsm, 1);
			if (!ret)
				otg->state = OTG_STATE_B_PERIPHERAL;
			else
				pr_err("OTG SM: cannot start gadget\n");
		}
		break;
	case OTG_STATE_B_PERIPHERAL:
		if (!fsm->id || !fsm->b_sess_vld) {
			ret = otg_start_gadget(fsm, 0);
			if (!ret)
				otg->state = OTG_STATE_B_IDLE;
			else
				pr_err("OTG SM: cannot stop gadget\n");
		}
		break;
	case OTG_STATE_A_IDLE:
		if (fsm->id) {
			otg->state = OTG_STATE_B_IDLE;
		} else {
			ret = otg_start_host(fsm, 1);
			if (!ret) {
				otg_drv_vbus(fsm, 1);
				otg->state = OTG_STATE_A_HOST;
			} else {
				pr_err("OTG SM: cannot start host\n");
			}
		}
		break;
	case OTG_STATE_A_HOST:
		if (fsm->id) {
			otg_drv_vbus(fsm, 0);
			ret = otg_start_host(fsm, 0);
			if (!ret)
				otg->state = OTG_STATE_A_IDLE;
			else
				pr_err("OTG SM: cannot stop host\n");
		}
		break;
	default:
		pr_err("OTG SM: invalid state\n");
	}

exit:
	if (!ret)
		ret = (otg->state != prev_state);

	pr_debug("OTG SM: %s => %s\n", usb_otg_state_string(prev_state),
		(ret > 0) ? usb_otg_state_string(otg->state) : "(no change)");

	return ret;
}

/* -------------------------------------------------------------------------- */

static struct dwc3_ext_otg_ops *dwc3_otg_exynos_rsw_probe(struct dwc3 *dwc)
{
	struct dwc3_ext_otg_ops *ops;
	bool ext_otg;

	ext_otg = dwc3_exynos_rsw_available(dwc->dev->parent);
	if (!ext_otg) {
		dev_err(dwc->dev, "failed to get ext_otg\n");
		return NULL;
	}

	/* Allocate and init otg instance */
	ops = devm_kzalloc(dwc->dev, sizeof(struct dwc3_ext_otg_ops),
			GFP_KERNEL);
	if (!ops) {
		 dev_err(dwc->dev, "unable to allocate dwc3_ext_otg_ops\n");
		 return NULL;
	}

	ops->setup = dwc3_exynos_rsw_setup;
	ops->exit = dwc3_exynos_rsw_exit;
	ops->start = dwc3_exynos_rsw_start;
	ops->stop = dwc3_exynos_rsw_stop;

	dev_err(dwc->dev, "%s done\n", __func__);

	return ops;
}

static void dwc3_otg_set_mode(struct dwc3 *dwc, u32 mode)
{
	u32 reg;

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~(DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG));
	reg |= DWC3_GCTL_PRTCAPDIR(mode);
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);
}

static void dwc3_otg_set_host_mode(struct dwc3_otg *dotg)
{
	struct dwc3 *dwc = dotg->dwc;
	u32 reg;

	if (dotg->regs) {
		reg = dwc3_readl(dotg->regs, DWC3_OCTL);
		reg &= ~DWC3_OTG_OCTL_PERIMODE;
		dwc3_writel(dotg->regs, DWC3_OCTL, reg);
	} else {
		dwc3_otg_set_mode(dwc, DWC3_GCTL_PRTCAP_HOST);
	}
}

static void dwc3_otg_set_peripheral_mode(struct dwc3_otg *dotg)
{
	struct dwc3 *dwc = dotg->dwc;
	u32 reg;

	if (dotg->regs) {
		reg = dwc3_readl(dotg->regs, DWC3_OCTL);
		reg |= DWC3_OTG_OCTL_PERIMODE;
		dwc3_writel(dotg->regs, DWC3_OCTL, reg);
	} else {
		dwc3_otg_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);
	}
}

static void dwc3_otg_drv_vbus(struct otg_fsm *fsm, int on)
{
	struct dwc3_otg	*dotg = container_of(fsm, struct dwc3_otg, fsm);
	int ret;

	if (IS_ERR(dotg->vbus_reg)) {
		dev_err(dotg->dwc->dev, "vbus regulator is not available\n");
		return;
	}

	if (on)
		ret = regulator_enable(dotg->vbus_reg);
	else
		ret = regulator_disable(dotg->vbus_reg);

	if (ret)
		dev_err(dotg->dwc->dev, "failed to turn Vbus %s\n",
						on ? "on" : "off");
}

int exynos_usbdrd_inform_dp_use(int use, int lane_cnt)
{
	int ret = 0;

	pr_info("[%s] dp use = %d, lane_cnt = %d\n",
			__func__, use, lane_cnt);

	if ((use == 1) && (lane_cnt == 4)) {
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		exynos_usbdrd_dp_use_notice(lane_cnt);
#endif
#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
		ret = xhci_portsc_set(0);
#endif
		udelay(1);
	}

	return ret;
}
EXPORT_SYMBOL(exynos_usbdrd_inform_dp_use);

static void dwc3_otg_qos_lock_delayed_work(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct dwc3_exynos *exynos = container_of(delay_work, struct dwc3_exynos,
					usb_qos_lock_delayed_work);
	struct dwc3_otg	*dotg = exynos->dotg;
	struct device *dev = dotg->dwc->dev;

	dev_info(dev, "%s\n", __func__);
	dwc3_exynos_set_bus_clock(exynos->dwc->dev, -1);
}

static void dwc3_int_lock_qos_work(struct work_struct *data)
{
	struct dwc3_exynos *exynos = container_of(data, struct dwc3_exynos, int_qos_work);
	struct dwc3_otg *dotg = exynos->dotg;
	struct device *dev = exynos->dev;

	if (dotg->pm_qos_int_val) {
		if (exynos->is_perf) {
			dev_info(dev, "int pm_qos set value = %d\n", dotg->pm_qos_int_val);
			exynos_pm_qos_update_request(&dotg->pm_qos_int_req, dotg->pm_qos_int_val);
		} else {
			dev_info(dev, "Reset int pm_qos setting\n");
			exynos_pm_qos_update_request(&dotg->pm_qos_int_req, 0);
		}
	}
}

void dwc3_otg_qos_lock(struct dwc3_exynos *exynos, int onoff)
{
	exynos->is_perf = !onoff;
	queue_work(exynos->int_qos_lock_wq, &exynos->int_qos_work);
}

void dwc3_otg_pm_ctrl(struct dwc3_exynos *dwc_exynos, int onoff)
{
	u32 reg;

	if (onoff == 0) {
		pr_info("Disable USB U1/U2 for performance.\n");
		/* Disable U1U2 */
		reg = dwc3_readl(dwc_exynos->dwc->regs, DWC3_DCTL);
		reg &= ~(DWC3_DCTL_INITU1ENA | DWC3_DCTL_ACCEPTU1ENA |
				DWC3_DCTL_INITU2ENA | DWC3_DCTL_ACCEPTU2ENA);
		dwc3_writel(dwc_exynos->dwc->regs, DWC3_DCTL, reg);
	} else {
		/* Enable U1U2 */
		reg = dwc3_readl(dwc_exynos->dwc->regs, DWC3_DCTL);
		reg |= (DWC3_DCTL_INITU1ENA | DWC3_DCTL_ACCEPTU1ENA |
				DWC3_DCTL_INITU2ENA | DWC3_DCTL_ACCEPTU2ENA);
		dwc3_writel(dwc_exynos->dwc->regs, DWC3_DCTL, reg);
	}

	dwc3_otg_qos_lock(dwc_exynos, onoff);
}

static void usb3_phy_control(struct dwc3_otg *dotg, int on)
{
	struct dwc3	*dwc = dotg->dwc;
	struct device	*dev = dwc->dev;

	dev_info(dev, "%s, USB3.0 PHY %s\n", __func__, on ? "on" : "off");

	if (on) {
		dwc3_core_susphy_set(dwc, 0);
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		exynos_usbdrd_pipe3_enable(dwc->usb3_generic_phy);
#endif
		dwc3_core_susphy_set(dwc, 1);
	} else {
		dwc3_core_susphy_set(dwc, 0);
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		exynos_usbdrd_pipe3_disable(dwc->usb3_generic_phy);
#endif
		dwc3_core_susphy_set(dwc, 1);
	}
}

void usb_dr_role_control(int on)
{
	struct dwc3	*dwc;
	dwc = g_dwc3_exynos->dwc;

	pr_info("%s: on: %d\n", __func__, on);

	if (on)
		dwc->current_dr_role = DWC3_GCTL_PRTCAP_DEVICE;
	else
		dwc->current_dr_role = DWC3_EXYNOS_IGNORE_CORE_OPS;

}
EXPORT_SYMBOL(usb_dr_role_control);

void usb_power_notify_control(int owner, int on)
{
	struct dwc3_otg	*dotg;
	struct dwc3	*dwc;
	struct device	*dev;
	u8 owner_bit = 0;

	if (g_dwc3_exynos == NULL) {
		pr_err("%s g_dwc3_exynos is NULL\n", __func__);
		return;
	}

	dwc = g_dwc3_exynos->dwc;

	if (dwc && g_dwc3_exynos->dotg && dwc->dev) {
		dotg = g_dwc3_exynos->dotg;
		dev = dwc->dev;
	} else {
		pr_err("%s dwc or dotg or dev NULL\n", __func__);
		return;
	}

	if (dwc->maximum_speed == USB_SPEED_HIGH) {
		dev_info(dev, "%s, Ignore USB3.0 phy control.\n");
		return;
	}

	dev_info(dev, "%s, combo phy = %d, owner= %d, on=%d\n",
		__func__, dotg->combo_phy_control, owner, on);

	mutex_lock(&dotg->lock);

	owner_bit = (1 << owner);

	if (on) {
		if (dotg->combo_phy_control == 0) {
			if (dotg->pm_qos_hsi0_val) {
				dev_info(dev, "pm_qos set value = %d\n",
						dotg->pm_qos_hsi0_val);
				exynos_pm_qos_update_request(&dotg->pm_qos_hsi0_req,
						dotg->pm_qos_hsi0_val);
			}
			usb3_phy_control(dotg, 1);
		}
		dotg->combo_phy_control |= owner_bit;
	} else {
		dotg->combo_phy_control &= ~(owner_bit);
		if (dotg->combo_phy_control == 0) {
			if (dotg->pm_qos_hsi0_val) {
				dev_info(dev, "Reset pm_qos setting\n");
				exynos_pm_qos_update_request(&dotg->pm_qos_hsi0_req, 0);
			}
			usb3_phy_control(dotg, 0);
		}
	}

	mutex_unlock(&dotg->lock);

	return;
}
EXPORT_SYMBOL(usb_power_notify_control);

void dwc3_otg_phy_tune(struct otg_fsm *fsm)
{
	struct usb_otg	*otg = fsm->otg;
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3	*dwc = dotg->dwc;

	exynos_usbdrd_phy_tune(dwc->usb2_generic_phy,
						dotg->otg.state);
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
	exynos_usbdrd_phy_tune(dwc->usb3_generic_phy,
						dotg->otg.state);
#endif
}
EXPORT_SYMBOL_GPL(dwc3_otg_phy_tune);

/*
 * dwc3_exynos_is_usb_ready - inform usb readiness to DP.
 */
int dwc3_otg_is_usb_ready(void)
{
	return g_dwc3_exynos->usb_host_ready;
}
EXPORT_SYMBOL_GPL(dwc3_otg_is_usb_ready);

static int dwc3_otg_start_host(struct otg_fsm *fsm, int on)
{
	struct usb_otg	*otg = fsm->otg;
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3	*dwc = dotg->dwc;
	struct device	*dev = dotg->dwc->dev;
	struct dwc3_exynos *exynos = dotg->exynos;
	struct device	*exynos_dev = exynos->dev;
	int ret = 0, i;
	int ret1 = -1;
	int wait_counter = 0;

#if defined(CONFIG_OTG_CDP_SUPPORT)
	union power_supply_propval val;
#endif
	if (!dotg->dwc->xhci) {
		dev_err(dev, "%s: does not have any xhci\n", __func__);
		return -EINVAL;
	}

	/* [W/A] Some device need VBUS up time */
	if (exynos->lazy_vbus_up)
		msleep(100);

	dev_info(dev, "Turn %s host\n", on ? "on" : "off");
	__pm_stay_awake(dotg->wakelock);

	if (on) {
		otg_connection = 1;
		dwc->gadget->deactivated = true;
		pr_info("%s: deactivate gadget!\n", __func__);
		if (dotg->pm_qos_hsi0_val) {
			dev_info(dev, "pm_qos set value = %d\n", dotg->pm_qos_hsi0_val);
			exynos_pm_qos_update_request(&dotg->pm_qos_hsi0_req,
							dotg->pm_qos_hsi0_val);
		}

#if defined(CONFIG_OTG_CDP_SUPPORT)
		/* vbus control test */
		pr_info("%s : psy_do_property vbus enable\n", __func__);
		val.intval = 1;
		psy_do_property("otg", set, POWER_SUPPLY_PROP_ONLINE, val);
		//-------------------//
		exynos_usbdrd_cdp_set(dwc->usb2_generic_phy, 1);
#endif
		pr_info("exynos RPM Usage Count: %d\n", exynos->dev->power.usage_count);
		wait_counter = 0;
		if (dotg->dwc3_suspended != USB_NORMAL) {
			while (!pm_runtime_suspended(exynos->dev)) {
				msleep(5);
				pr_info("%s: wait AP resume, %d!\n", __func__, wait_counter++);
				if (wait_counter >= 10) {
					pr_info("%s: Can't wait AP resume break!\n", __func__);
					break;
				}
			}
		}
		dotg->dwc3_suspended = USB_NORMAL;
		ret = pm_runtime_get_sync(exynos_dev);
		if (ret < 0) {
			dev_err(dwc->dev, "%s: failed to initialize exynos: %d\n",
					__func__, ret);
		}
		ret = pm_runtime_get_sync(dev);
		if (ret < 0) {
			dev_err(dwc->dev, "%s: failed to initialize core\n",
					__func__);
			goto err;
		}

		dwc3_otg_phy_tune(fsm);

		dwc3_exynos_core_init(dwc, exynos);

		dwc3_otg_set_host_mode(dotg);

		exynos->usb_host_ready = true;

		ret = platform_device_add(dwc->xhci);
		if (ret) {
			dev_err(dev, "%s: cannot add xhci\n", __func__);
			goto err;
		}

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
		usbpd_set_host_on(dotg->man, on);
#endif
	} else {
		otg_connection = 0;

		exynos->usb_host_ready = false;

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
		usbpd_set_host_on(dotg->man, on);
#endif
		if (dotg->dwc3_suspended == USB_SUSPEND_PREPARE) {
			pr_info("%s: wait resume completion\n", __func__);
			ret1 = wait_for_completion_timeout(&dotg->resume_cmpl,
							msecs_to_jiffies(5000));
		}

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
		/* USB audio disconnect progressing */
		for (i = 0; i < 1000; i += 100) {
			if (usb_audio_connection) {
				usleep_range(100000, 110000);
				pr_info("%s: wait audio	disconnect\n", __func__);
			} else {
				pr_info("%s: audio disconnect DONE!!\n",__func__);
				break;
			}
		}
#endif

		platform_device_del(dwc->xhci);
		dwc->xhci->dev.p->dead = 0;

		pm_runtime_put_sync_suspend(dev);
		pm_runtime_put_sync_suspend(exynos_dev);
err:
		dwc->gadget->deactivated = false;
		pr_info("%s: activate gadget!\n", __func__);
		otg_connection = 0;

#if defined(CONFIG_OTG_CDP_SUPPORT)
		pr_info("%s : psy_do_property vbus disable\n", __func__);
		val.intval = 0;
		psy_do_property("otg", set, POWER_SUPPLY_PROP_ONLINE, val);
		exynos_usbdrd_cdp_set(dwc->usb2_generic_phy, 0);
#endif
		if (dotg->pm_qos_hsi0_val) {
			dev_info(dev, "pm_qos reset\n");
			exynos_pm_qos_update_request(&dotg->pm_qos_hsi0_req, 0);
		}
	}
	__pm_relax(dotg->wakelock);
	return ret;
}


u8 dwc3_otg_get_link_state(struct dwc3 *dwc)
{
	u32			reg;
	u8			link_state;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	link_state = DWC3_DSTS_USBLNKST(reg);

	return link_state;
}
EXPORT_SYMBOL_GPL(dwc3_otg_get_link_state);

static int dwc3_check_extra_work(struct dwc3 *dwc)
{
	struct usb_gadget *gadget = dwc->gadget;
	struct usb_composite_dev *cdev;
	struct usb_function *f;
	struct device *dev = dwc->dev;
	int i, ret = 0;

	if (gadget == NULL) {
		pr_err("%s : Can't check extra work. gadget is NULL\n", __func__);
		return 0;
	}

	cdev = get_gadget_data(gadget);

	if (cdev == NULL) {
		pr_err("%s : Can't check extra work. cdev is NULL\n", __func__);
		return 0;
	}

	for (i = 0; i < MAX_CONFIG_INTERFACES; i++) {
		if (cdev->config == NULL)
			break;

		f = cdev->config->interface[i];
		if (f == NULL)
			break;

		/* Exynos specific function need extra delay */
		if (strncmp(f->name, "dm", sizeof("dm")) == 0) {
			dev_info(dev, "found dm function...\n");
			/* f->disable(f); */
			ret = 1;
		} else if (strncmp(f->name, "acm", sizeof("acm")) == 0) {
			dev_info(dev, "found acm function...\n");
			/* f->disable(f); */
			ret = 1;
		}
	}

	return ret;
}

static int dwc3_otg_start_gadget(struct otg_fsm *fsm, int on)
{
	struct usb_otg	*otg = fsm->otg;
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct dwc3	*dwc = dotg->dwc;
	struct dwc3_exynos *exynos = dotg->exynos;
	struct device	*dev = dotg->dwc->dev;
	struct device	*exynos_dev = exynos->dev;
	int ret = 0;
	int wait_counter = 0, extra_delay = 0;
	u32 evt_count, evt_buf_cnt;

	if (!otg->gadget) {
		dev_err(dev, "%s does not have any gadget\n", __func__);
		return -EINVAL;
	}

	dev_info(dev, "Turn %s gadget %s\n",
			on ? "on" : "off", otg->gadget->name);
#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
	vbus_session_notify(dwc->gadget, on, EAGAIN);
#endif
	if (on) {
		__pm_stay_awake(dotg->wakelock);
		if (dotg->pm_qos_hsi0_val) {
			dev_info(dev, "pm_qos set value = %d\n", dotg->pm_qos_hsi0_val);
			exynos_pm_qos_update_request(&dotg->pm_qos_hsi0_req,
							dotg->pm_qos_hsi0_val);
		}
		exynos->vbus_state = true;
		dwc->ev_buf->flags |= BIT(20);
		pr_info("%s: set BIT(20) event buffer flags\n", __func__);

		while (dwc->gadget_driver == NULL) {
			wait_counter++;
			usleep_range(100, 200);

			if (wait_counter > 500) {
				pr_err("Can't wait gadget start!\n");
				break;
			}
		}

		pr_info("exynos RPM Usage Count: %d\n", exynos->dev->power.usage_count);
		wait_counter = 0;
		if (dotg->dwc3_suspended != USB_NORMAL) {
			while (!pm_runtime_suspended(exynos->dev)) {
				msleep(5);
				pr_info("%s: wait AP resume, %d!\n", __func__, wait_counter++);
				if (wait_counter >= 10) {
					pr_info("%s: Can't wait AP resume break!\n", __func__);
					break;
				}
			}
		}
		dotg->dwc3_suspended = USB_NORMAL;

		ret = pm_runtime_get_sync(exynos_dev);
		if (ret < 0) {
			dev_err(dwc->dev, "%s: failed to initialize exynos : ret\n",
					__func__, ret);
		}
		ret = pm_runtime_get_sync(dev);
		if (ret < 0) {
			dev_err(dwc->dev, "%s: failed to initialize core : ret\n",
					__func__, ret);
		}

		dwc3_otg_phy_tune(fsm);

		dwc3_exynos_core_init(dwc, exynos);

		dwc3_otg_set_peripheral_mode(dotg);

#if IS_ENABLED(CONFIG_USB_EXYNOS_TPMON_MODULE)
		usb_tpmon_open();
#endif
	} else {
		exynos->vbus_state = false;
		dwc->ev_buf->flags &= ~BIT(20);
		pr_info("%s: clear BIT(20) event buffer flags\n", __func__);

#if IS_ENABLED(CONFIG_USB_EXYNOS_TPMON_MODULE)
		usb_tpmon_close();
#endif

		evt_buf_cnt = dwc->ev_buf->count;

		/* Wait until gadget stop */
		wait_counter = 0;
		evt_count = dwc3_readl(dwc->regs, DWC3_GEVNTCOUNT(0));
		evt_count &= DWC3_GEVNTCOUNT_MASK;
		while (evt_count || evt_buf_cnt) {
			wait_counter++;
			mdelay(20);

			if (wait_counter > 20) {
				pr_err("Can't wait dwc disconnect!\n");
				break;
			}
			evt_count = dwc3_readl(dwc->regs, DWC3_GEVNTCOUNT(0));
			evt_count &= DWC3_GEVNTCOUNT_MASK;
			evt_buf_cnt = dwc->ev_buf->count;
			dev_dbg(dev, "%s: evt = %d, evt_buf cnt = %d\n",
				__func__, evt_count, evt_buf_cnt);
		}
		dev_info(dev, "%s, evt compl wait cnt = %d\n",
			 __func__, wait_counter);

		/*
		 * we can extra work corresponding each functions by
		 * the following function.
		 */
		dwc3_exynos_gadget_disconnect_proc(dwc);

		extra_delay = dwc3_check_extra_work(dwc);
		if (extra_delay)
			mdelay(100);

		pm_runtime_put_sync_suspend(dev);
		pm_runtime_put_sync_suspend(exynos_dev);
		if (dotg->pm_qos_hsi0_val) {
			dev_info(dev, "pm_qos reset\n");
			exynos_pm_qos_update_request(&dotg->pm_qos_hsi0_req, 0);
		}
		__pm_relax(dotg->wakelock);
	}

	return 0;
}

static struct otg_fsm_ops dwc3_otg_fsm_ops = {
	.drv_vbus	= dwc3_otg_drv_vbus,
	.start_host	= dwc3_otg_start_host,
	.start_gadget	= dwc3_otg_start_gadget,
};

/* -------------------------------------------------------------------------- */

void dwc3_otg_run_sm(struct otg_fsm *fsm)
{
	struct dwc3_otg	*dotg = container_of(fsm, struct dwc3_otg, fsm);
	struct dwc3 *dwc = dotg->dwc;
	int state_changed;
	int i;

	/* Prevent running SM on early system resume */
	if (!dotg->ready)
		return;

	mutex_lock(&dwc->mutex);
	for (i = 0; i < 100; i++) {
		if (dotg->dwc3_suspended == USB_SUSPEND_PREPARE) {
			pr_info("%s Waiting system resume!!\n", __func__);
			msleep(50);
		} else {
			pr_info("%s System resume Done!!\n", __func__);
			break;
		}
	}

	mutex_lock(&fsm->lock);

	for (i = 0; i < MAX_STATE_MACHINE_CNT; i++) {
		state_changed = dwc3_otg_statemachine(fsm);
		if (state_changed <= 0) break; // success
	}
	if (i == MAX_STATE_MACHINE_CNT)
		pr_info("%s may be failed\n", __func__);

	mutex_unlock(&fsm->lock);
	mutex_unlock(&dwc->mutex);
}
EXPORT_SYMBOL_GPL(dwc3_otg_run_sm);

/* Bind/Unbind the peripheral controller driver */
static int dwc3_otg_set_peripheral(struct usb_otg *otg,
				struct usb_gadget *gadget)
{
	struct dwc3_otg	*dotg = container_of(otg, struct dwc3_otg, otg);
	struct otg_fsm	*fsm = &dotg->fsm;
	struct dwc3 *dwc = dotg->dwc;
	struct device	*dev = dotg->dwc->dev;

	if (gadget) {
		dev_info(dev, "Binding gadget %s\n", gadget->name);

		otg->gadget = gadget;
	} else {
		dev_info(dev, "Unbinding gadget\n");

		mutex_lock(&dwc->mutex);
		mutex_lock(&fsm->lock);

		if (otg->state == OTG_STATE_B_PERIPHERAL) {
			/* Reset OTG Statemachine */
			dotg->fsm_reset = 1;
			dwc3_otg_statemachine(fsm);
			dotg->fsm_reset = 0;
		}
		otg->gadget = NULL;

		mutex_unlock(&fsm->lock);
		mutex_unlock(&dwc->mutex);

		dwc3_otg_run_sm(fsm);
	}

	return 0;
}

static int dwc3_otg_get_id_state(struct dwc3_otg *dotg)
{
	u32 reg = dwc3_readl(dotg->regs, DWC3_OSTS);

	return !!(reg & DWC3_OTG_OSTS_CONIDSTS);
}

static int dwc3_otg_get_b_sess_state(struct dwc3_otg *dotg)
{
	u32 reg = dwc3_readl(dotg->regs, DWC3_OSTS);

	return !!(reg & DWC3_OTG_OSTS_BSESVALID);
}

static irqreturn_t dwc3_otg_interrupt(int irq, void *_dotg)
{
	struct dwc3_otg	*dotg = (struct dwc3_otg *)_dotg;
	struct otg_fsm	*fsm = &dotg->fsm;
	u32 oevt, handled_events = 0;
	irqreturn_t ret = IRQ_NONE;

	oevt = dwc3_readl(dotg->regs, DWC3_OEVT);

	/* ID */
	if (oevt & DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT) {
		fsm->id = dwc3_otg_get_id_state(dotg);
		handled_events |= DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT;
	}

	/* VBus */
	if (oevt & DWC3_OEVTEN_OTGBDEVVBUSCHNGEVNT) {
		fsm->b_sess_vld = dwc3_otg_get_b_sess_state(dotg);
		handled_events |= DWC3_OEVTEN_OTGBDEVVBUSCHNGEVNT;
	}

	if (handled_events) {
		dwc3_writel(dotg->regs, DWC3_OEVT, handled_events);
		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static irqreturn_t dwc3_otg_thread_interrupt(int irq, void *_dotg)
{
	struct dwc3_otg	*dotg = (struct dwc3_otg *)_dotg;

	dwc3_otg_run_sm(&dotg->fsm);

	return IRQ_HANDLED;
}

static void dwc3_otg_enable_irq(struct dwc3_otg *dotg)
{
	/* Enable only connector ID status & VBUS change events */
	dwc3_writel(dotg->regs, DWC3_OEVTEN,
			DWC3_OEVTEN_OTGCONIDSTSCHNGEVNT |
			DWC3_OEVTEN_OTGBDEVVBUSCHNGEVNT);
}

static void dwc3_otg_disable_irq(struct dwc3_otg *dotg)
{
	dwc3_writel(dotg->regs, DWC3_OEVTEN, 0x0);
}

static void dwc3_otg_reset(struct dwc3_otg *dotg)
{
	/*
	 * OCFG[2] - OTG-Version = 0
	 * OCFG[1] - HNPCap = 0
	 * OCFG[0] - SRPCap = 0
	 */
	dwc3_writel(dotg->regs, DWC3_OCFG, 0x0);

	/*
	 * OCTL[6] - PeriMode = 1
	 * OCTL[5] - PrtPwrCtl = 0
	 * OCTL[4] - HNPReq = 0
	 * OCTL[3] - SesReq = 0
	 * OCTL[2] - TermSelDLPulse = 0
	 * OCTL[1] - DevSetHNPEn = 0
	 * OCTL[0] - HstSetHNPEn = 0
	 */
	dwc3_writel(dotg->regs, DWC3_OCTL, DWC3_OTG_OCTL_PERIMODE);

	/* Clear all otg events (interrupts) indications  */
	dwc3_writel(dotg->regs, DWC3_OEVT, (u32)DWC3_OEVT_CLEAR_ALL);
}

/* -------------------------------------------------------------------------- */

static ssize_t
dwc3_otg_show_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct usb_otg		*otg = &exynos->dotg->otg;

	return snprintf(buf, PAGE_SIZE, "%s\n",
			usb_otg_state_string(otg->state));
}

static DEVICE_ATTR(state, S_IRUSR | S_IRGRP,
	dwc3_otg_show_state, NULL);

static ssize_t
dwc3_otg_show_b_sess(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct otg_fsm	*fsm = &exynos->dotg->fsm;

	return snprintf(buf, PAGE_SIZE, "%d\n", fsm->b_sess_vld);
}

static ssize_t
dwc3_otg_store_b_sess(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct otg_fsm	*fsm = &exynos->dotg->fsm;
	int		b_sess_vld;

	if (sscanf(buf, "%d", &b_sess_vld) != 1)
		return -EINVAL;

	fsm->b_sess_vld = !!b_sess_vld;

	dwc3_otg_run_sm(fsm);

	return n;
}

static DEVICE_ATTR(b_sess, S_IWUSR | S_IRUSR | S_IRGRP,
	dwc3_otg_show_b_sess, dwc3_otg_store_b_sess);

static ssize_t
dwc3_otg_show_id(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct otg_fsm	*fsm = &exynos->dotg->fsm;

	return snprintf(buf, PAGE_SIZE, "%d\n", fsm->id);
}

static ssize_t
dwc3_otg_store_id(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct otg_fsm	*fsm = &exynos->dotg->fsm;
	int id;

	if (sscanf(buf, "%d", &id) != 1)
		return -EINVAL;

	fsm->id = !!id;

	dwc3_otg_run_sm(fsm);

	return n;
}

static DEVICE_ATTR(id, S_IWUSR | S_IRUSR | S_IRGRP,
	dwc3_otg_show_id, dwc3_otg_store_id);


static struct attribute *dwc3_otg_attributes[] = {
	&dev_attr_id.attr,
	&dev_attr_b_sess.attr,
	&dev_attr_state.attr,
	NULL
};

static const struct attribute_group dwc3_otg_attr_group = {
	.attrs = dwc3_otg_attributes,
};

/**
 * dwc3_otg_start
 * @dwc: pointer to our controller context structure
 */
int dwc3_otg_start(struct dwc3 *dwc, struct dwc3_exynos *exynos)
{
	struct dwc3_otg	*dotg = exynos->dotg;
	struct otg_fsm	*fsm = &dotg->fsm;
	int		ret;

	if (dotg->ext_otg_ops) {
		ret = dwc3_ext_otg_start(dotg);
		if (ret) {
			dev_err(dwc->dev, "failed to start external OTG\n");
			return ret;
		}
	} else {
		dotg->regs = dwc->regs;

		dwc3_otg_reset(dotg);

		dotg->fsm.id = dwc3_otg_get_id_state(dotg);
		dotg->fsm.b_sess_vld = dwc3_otg_get_b_sess_state(dotg);

		dotg->irq = platform_get_irq(to_platform_device(dwc->dev), 0);
		ret = devm_request_threaded_irq(dwc->dev, dotg->irq,
				dwc3_otg_interrupt,
				dwc3_otg_thread_interrupt,
				IRQF_SHARED, "dwc3-otg", dotg);
		if (ret) {
			dev_err(dwc->dev, "failed to request irq #%d --> %d\n",
					dotg->irq, ret);
			return ret;
		}

		dwc3_otg_enable_irq(dotg);
	}

	dotg->ready = 1;

	dwc3_otg_run_sm(fsm);

	return 0;
}

/**
 * dwc3_otg_stop
 * @dwc: pointer to our controller context structure
 */
void dwc3_otg_stop(struct dwc3 *dwc, struct dwc3_exynos *exynos)
{
	struct dwc3_otg         *dotg = exynos->dotg;

	if (dotg->ext_otg_ops) {
		dwc3_ext_otg_stop(dotg);
	} else {
		dwc3_otg_disable_irq(dotg);
		free_irq(dotg->irq, dotg);
	}

	dotg->ready = 0;
}

/* -------------------------------------------------------------------------- */
u32 otg_is_connect(void)
{
	if (!otg_connection)
		return OTG_NO_CONNECT;
	else if (is_otg_only)
		return OTG_CONNECT_ONLY;
	else
		return OTG_DEVICE_CONNECT;
}
EXPORT_SYMBOL_GPL(otg_is_connect);

int get_idle_ip_index(void)
{
	if (g_dwc3_exynos == NULL) {
		pr_info("Idle ip index will be set next time.\n");
		return -ENODEV;
	}
	return dwc3_exynos_get_idle_ip_index(g_dwc3_exynos->dev);

}
EXPORT_SYMBOL_GPL(get_idle_ip_index);

static int dwc3_otg_pm_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct dwc3_otg *dotg
		= container_of(nb, struct dwc3_otg, pm_nb);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		pr_info("%s suspend prepare\n", __func__);
		dotg->dwc3_suspended = USB_SUSPEND_PREPARE;
		reinit_completion(&dotg->resume_cmpl);
		break;
	case PM_POST_SUSPEND:
		pr_info("%s post suspend\n", __func__);
		dotg->dwc3_suspended = USB_POST_SUSPEND;
		complete(&dotg->resume_cmpl);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

#if defined(CONFIG_OTG_DEFAULT)
static void typec_work_func(struct work_struct *work)
{
	struct dwc3_otg		*dotg = container_of(work, struct dwc3_otg,
							typec_work.work);
	struct dwc3		*dwc = dotg->dwc;
	struct intf_typec	*typec = dotg->typec;
	struct typec_partner_desc partner;

	pr_info("%s\n", __func__);

	typec->cap.type = TYPEC_PORT_DRP;
	typec->cap.revision = USB_TYPEC_REV_1_2;
	typec->cap.pd_revision = 0x312;
	typec->cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;

	typec->port = typec_register_port(dwc->dev, &typec->cap);
	if (!typec->port) {
		dev_err(dwc->dev, "failed register port\n");
		return;
	}

	typec_set_data_role(typec->port, TYPEC_DEVICE);
	typec_set_pwr_role(typec->port, TYPEC_SINK);
	typec_set_pwr_opmode(typec->port, TYPEC_PWR_MODE_USB);

	memset(&partner, 0, sizeof(struct typec_partner_desc));

	typec->partner = typec_register_partner(typec->port, &partner);
	if (!dotg->typec->partner)
		dev_err(dwc->dev, "failed register partner\n");
}
#endif

static int usb_reboot_noti(struct notifier_block *nb, unsigned long event, void *buf)
{
	struct dwc3_otg *dotg = g_dwc3_exynos->dotg;
	struct dwc3	*dwc = dotg->dwc;
	struct otg_fsm  *fsm = &dotg->fsm;
	int ret = 0;

	dev_info(dwc->dev, "%s, event = %d\n", __func__, event);

	switch (event) {
	case SYS_RESTART:
	case SYS_POWER_OFF:
		exynos_usbdrd_shutdown_notice(1);
		if (otg_connection == 1) {
			dev_info(dwc->dev, "host enabled. Turn off host\n");
			fsm->id = 1;
			dwc3_otg_run_sm(fsm);
		}
		break;
	}

	return ret;
}

int dwc3_exynos_otg_init(struct dwc3 *dwc, struct dwc3_exynos *exynos)
{
	struct dwc3_otg *dotg;
	struct dwc3_ext_otg_ops *ops = NULL;
	int ret = 0;
#if defined(CONFIG_OTG_DEFAULT)
	struct intf_typec	*typec;
#endif

	dev_info(dwc->dev, "%s\n", __func__);

	/* EXYNOS SoCs don't have HW OTG, but it supports SW OTG. */
	ops = dwc3_otg_exynos_rsw_probe(dwc);
	if (!ops)
		return 0;

	g_dwc3_exynos = exynos;

	/* Allocate and init otg instance */
	dotg = devm_kzalloc(dwc->dev, sizeof(struct dwc3_otg), GFP_KERNEL);
	if (!dotg) {
		dev_err(dwc->dev, "unable to allocate dwc3_otg\n");
		return -ENOMEM;
	}

	/* This reference is used by dwc3 modules for checking otg existance */
	exynos->dotg = dotg;
	dotg->dwc = dwc;
	dotg->exynos = exynos;
	dev_info(dwc->dev, "%s, dotg = %8x\n", __func__, exynos->dotg);

	ret = of_property_read_u32(dwc->dev->of_node,
				"usb-pm-qos-hsi0", &dotg->pm_qos_hsi0_val);
	if (ret < 0) {
		dev_err(dwc->dev, "couldn't read usb-pm-qos-hsi0 %s node, error = %d\n",
					dwc->dev->of_node->name, ret);
		dotg->pm_qos_hsi0_val = 0;
	} else {
		exynos_pm_qos_add_request(&dotg->pm_qos_hsi0_req,
					PM_QOS_HSI0_THROUGHPUT, 0);
	}

	ret = of_property_read_u32(dwc->dev->of_node,
				"usb-pm-qos-int", &dotg->pm_qos_int_val);
	if (ret < 0) {
		dev_err(dwc->dev, "couldn't read usb-pm-qos-int %s node, error = %d\n",
					dwc->dev->of_node->name, ret);
		dotg->pm_qos_int_val = 0;
	} else {
		exynos_pm_qos_add_request(&dotg->pm_qos_int_req,
					PM_QOS_DEVICE_THROUGHPUT, 0);
	}

	dotg->ext_otg_ops = ops;

	dotg->otg.set_peripheral = dwc3_otg_set_peripheral;
	dotg->otg.set_host = NULL;

	dotg->otg.state = OTG_STATE_UNDEFINED;

	mutex_init(&dotg->fsm.lock);
	dotg->fsm.ops = &dwc3_otg_fsm_ops;
	dotg->fsm.otg = &dotg->otg;

	dotg->vbus_reg = devm_regulator_get(dwc->dev, "dwc3-vbus");
	if (IS_ERR(dotg->vbus_reg))
		dev_err(dwc->dev, "failed to obtain vbus regulator\n");

	if (dotg->ext_otg_ops) {
		dev_info(dwc->dev, "%s, dwc3_ext_otg_setup call\n", __func__);
		ret = dwc3_ext_otg_setup(dotg);
		if (ret) {
			dev_err(dwc->dev, "failed to setup OTG\n");
			return ret;
		}
	}

	dotg->wakelock = wakeup_source_register(dwc->dev, "dwc3-otg");
	mutex_init(&dotg->lock);

	ret = sysfs_create_group(&exynos->dev->kobj, &dwc3_otg_attr_group);
	if (ret)
		dev_err(dwc->dev, "failed to create dwc3 otg attributes\n");

	init_completion(&dotg->resume_cmpl);
	dotg->dwc3_suspended = USB_NORMAL;
	dotg->pm_nb.notifier_call = dwc3_otg_pm_notifier;
	register_pm_notifier(&dotg->pm_nb);

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	dotg->usb_d.data = (void *)dotg;
	dotg->man = register_usb(&dotg->usb_d);
#endif

	/* register_usb_is_connect(otg_is_connect); */

	exynos->int_qos_lock_wq = alloc_ordered_workqueue("usb_int_qos_wq",
					WQ_FREEZABLE | WQ_MEM_RECLAIM);
	INIT_WORK(&exynos->int_qos_work, dwc3_int_lock_qos_work);
	INIT_DELAYED_WORK(&exynos->usb_qos_lock_delayed_work,
					dwc3_otg_qos_lock_delayed_work);
#if defined(CONFIG_OTG_DEFAULT)
	INIT_DELAYED_WORK(&dotg->typec_work, typec_work_func);

	typec = devm_kzalloc(dwc->dev, sizeof(*typec), GFP_KERNEL);
	if (!typec)
		return -ENOMEM;

	/* mutex_init(&md05->lock); */
	typec->dev = dwc->dev;
	dotg->typec = typec;

	schedule_delayed_work(&dotg->typec_work,
			      msecs_to_jiffies(2000));
#endif

	dev_info(dwc->dev, "%s done\n", __func__);
#if IS_ENABLED(CONFIG_USB_EXYNOS_TPMON_MODULE)
	usb_tpmon_init();
#endif
	ret = register_reboot_notifier(&usb_reboot_notifier);
	if (ret)
		dev_err(dwc->dev, "failed register reboot notifier\n");


	return 0;
}

void dwc3_exynos_otg_exit(struct dwc3 *dwc, struct dwc3_exynos *exynos)
{
	struct dwc3_otg *dotg = exynos->dotg;

	if (!dotg->ext_otg_ops)
		return;

#if defined(CONFIG_OTG_DEFAULT)
	typec_unregister_partner(dotg->typec->partner);
	typec_unregister_port(dotg->typec->port);
#endif
	unregister_pm_notifier(&dotg->pm_nb);

	dwc3_ext_otg_exit(dotg);

	sysfs_remove_group(&dwc->dev->kobj, &dwc3_otg_attr_group);
	wakeup_source_unregister(dotg->wakelock);
	free_irq(dotg->irq, dotg);
	dotg->otg.state = OTG_STATE_UNDEFINED;
	kfree(dotg);
	exynos->dotg = NULL;
#if IS_ENABLED(CONFIG_USB_EXYNOS_TPMON_MODULE)
	usb_tpmon_exit();
#endif
}
