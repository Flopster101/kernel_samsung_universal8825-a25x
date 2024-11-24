// SPDX-License-Identifier: GPL-2.0
/*
 * muic_platform_layer.c
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#define pr_fmt(fmt)	"[MUIC] " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/muic/slsi/platform/muic_platform_layer.h>
#include <linux/muic/slsi/platform/muic_platform_manager.h>

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */
#include <linux/muic/common/muic_sysfs.h>
#include <linux/muic/common/muic.h>

#include <linux/usb/typec/common/pdic_param.h>
#include <linux/usb_notify.h>

struct m_p_l_data {
	struct muic_platform_data *pdata;
	struct muic_share_data *sdata;
};

static struct m_p_l_data *mpl_data;
extern struct muic_platform_data muic_pdata;

static const char *const pdic_event[] = {
	[EVENT_PDIC_RPRD] = "RPRD",
	[EVENT_PDIC_ATTACH] = "ATTACH",
	[EVENT_PDIC_DETACH] = "DETACH",
	[EVENT_PDIC_RID] = "RID",
	[EVENT_PDIC_WATER] = "WATER",
	[EVENT_PDIC_TA] = "TA",
	[EVENT_PDIC_OTG] = "OTG",
	[EVENT_PDIC_ROLE_SWAP] = "ROLE_SWAP",
};
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static int muic_platform_set_overheat_hiccup(void *data, int en);
static int muic_platform_set_hiccup(void *data, int en);
static int muic_platform_get_hiccup(void *data);
#endif
#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
static int muic_platform_get_vbus_value(void *data);
static int muic_platform_get_adc(void *data)
{
	struct muic_share_data *sdata = data;
	struct muic_ic_data *ic_data;
	int ret = 0;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	MUIC_PDATA_FUNC(ic_data->m_ops.get_adc, ic_data->drv_data, &ret);
err:
	return ret;
}
#endif

static int muic_platform_get_vbus_state(struct muic_share_data *sdata)
{
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	int ret = 0;

	MUIC_PDATA_FUNC(ic_data->m_ops.get_vbus_state, ic_data->drv_data, &ret);
	pr_info("%s vbus state=%d\n", __func__, ret);

	if (ret < 0)
		pr_info("%s callback function error! ret=%d\n", __func__, ret);

	return ret;
}

static int muic_platform_set_switch_path(struct muic_share_data *sdata, int path)
{
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	int ret = 0;

	pr_info("%s path=%d\n", __func__, path);

	switch (path) {
	case MUIC_PATH_OPEN:
		MUIC_PDATA_FUNC(ic_data->m_ops.set_switch_to_open, ic_data->drv_data, &ret);
		break;
	case MUIC_PATH_USB_AP:
	case MUIC_PATH_USB_CP:
		MUIC_PDATA_FUNC_MULTI_PARAM(ic_data->m_ops.set_switch_to_usb,
			ic_data->drv_data, path, &ret);
		break;
	case MUIC_PATH_UART_AP:
	case MUIC_PATH_UART_CP:
		MUIC_PDATA_FUNC_MULTI_PARAM(ic_data->m_ops.set_switch_to_uart,
			ic_data->drv_data, path, &ret);
		break;
	default:
		pr_info("%s A wrong com path!!\n", __func__);
	}

	if (ret < 0)
		pr_info("%s callback function error! ret=%d\n", __func__, ret);

	return ret;
}

static void muic_platform_switch_uart_path(struct muic_share_data *sdata)
{
	struct muic_platform_data *pdata = sdata->pdata;

	pr_info("%s uart_path=(%d)\n", __func__, pdata->uart_path);

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
	case MUIC_PATH_UART_CP:
		muic_platform_set_switch_path(sdata, pdata->uart_path);
		break;
	default:
		pr_info("%s invalid uart_path\n", __func__);
		break;
	}
}

static void muic_platform_switch_usb_path(struct muic_share_data *sdata)
{
	struct muic_platform_data *pdata = sdata->pdata;

	pr_info("%s usb_path=(%d)\n", __func__, pdata->usb_path);

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
	case MUIC_PATH_USB_CP:
		muic_platform_set_switch_path(sdata, pdata->usb_path);
		break;
	default:
		pr_info("%s invalid usb_path\n", __func__);
		break;
	}
}

static int muic_platform_handle_pdic_rprd(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	muic_attached_dev_t new_dev = ATTACHED_DEV_OTG_MUIC;
	int ret = 0;

	if (sdata->attached_dev == new_dev) {
		pr_err("%s skip to handle duplicated event\n", __func__);
		ret = EPERM;
		goto err;
	}

	pr_info("%s +\n", __func__);

	muic_platform_handle_attach(sdata, new_dev);
	muic_platform_switch_usb_path(sdata);
	pr_info("%s -\n", __func__);
err:
	return ret;
}

static int muic_platform_handle_pdic_attach(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	int ret = 0;

	sdata->is_pdic_attached = true;
	return ret;
}

static int muic_platform_handle_pdic_detach(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	int ret = 0;
	int vbus = 0;

	pr_info("%s sdata->attached_dev=(%d)\n", __func__, sdata->attached_dev);

	vbus = muic_platform_get_vbus_state(sdata);
	MUIC_PDATA_VOID_FUNC_MULTI_PARAM(ic_data->m_ops.set_chg_det, ic_data->drv_data, true);
	sdata->is_afc_pdic_ready = false;
	sdata->is_pdic_attached = false;
	if (vbus == 0)
		muic_platform_handle_detach(sdata);

	return ret;
}

static int muic_platform_handle_pdic_rid(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	muic_attached_dev_t attached_dev = ATTACHED_DEV_NONE_MUIC;
	int rid, vbus, ret = 0;

	vbus = muic_platform_get_vbus_state(sdata);
	rid = muic_if->pdic->pdic_evt_rid;

	switch (rid) {
	case PDIC_RID_000K:
		attached_dev = ATTACHED_DEV_OTG_MUIC;
		break;
	case PDIC_RID_255K:
		attached_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		break;
	case PDIC_RID_301K:
		attached_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		break;
	case PDIC_RID_523K:
		attached_dev = (vbus) ? ATTACHED_DEV_JIG_UART_OFF_VB_MUIC : ATTACHED_DEV_JIG_UART_OFF_MUIC;
		break;
	case PDIC_RID_619K:
		attached_dev = (vbus) ? ATTACHED_DEV_JIG_UART_ON_VB_MUIC : ATTACHED_DEV_JIG_UART_ON_MUIC;
		break;
	case PDIC_RID_OPEN:
		attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	default:
		break;
	}

	muic_platform_handle_attach(sdata, attached_dev);
	return ret;
}

static int muic_platform_handle_pdic_water(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	int ret = 0;

	if (!sdata) {
		pr_err("%s sdata is null\n", __func__);
		goto err;
	}
	pr_info("%s is_water=(%d)\n", __func__, muic_if->is_water);

	if (muic_if->is_water) {
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
		if (sdata->is_hiccup_mode == false) {
			pr_info("%s set hiccup path\n", __func__);
			muic_platform_set_hiccup(sdata, true);
		}
#endif
	}
err:
	return ret;
}

static int muic_platform_handle_pdic_ta(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	struct muic_ic_data *ic_data = sdata->ic_data;
	int ret = 0;

	sdata->is_afc_pdic_ready = true;
	MUIC_PDATA_FUNC(ic_data->m_ops.set_afc_ready, ic_data->drv_data, &ret);

	return ret;
}

static int muic_platform_handle_pdic_otg(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	muic_attached_dev_t new_dev = ATTACHED_DEV_OTG_MUIC;
	int ret = 0;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	pr_info("%s +\n", __func__);
	sdata->is_pdic_attached = true;
	MUIC_PDATA_FUNC(ic_data->m_ops.check_usb_killer, ic_data->drv_data, &ret);
	if (ret == MUIC_NORMAL_OTG) {
		muic_platform_handle_attach(sdata, new_dev);
		MUIC_PDATA_VOID_FUNC_MULTI_PARAM(ic_data->m_ops.set_chg_det, ic_data->drv_data, false);
		MUIC_SEND_NOTI_TO_PDIC_ATTACH(ATTACHED_DEV_OTG_MUIC);
		muic_platform_switch_usb_path(sdata);
		pr_info("%s -\n", __func__);
	} else {
		pr_info("[MUIC] %s USB Killer Detected!!!\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_USBKILLER;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_USB_KILLER_COUNT);
#endif
	}
	return ret;
}

static int muic_platform_handle_pdic_role_swap(struct muic_interface_t *muic_if)
{
	struct muic_share_data *sdata = muic_if->sdata;
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	int ret = 0;

	pr_info("%s +\n", __func__);

	MUIC_PDATA_VOID_FUNC_MULTI_PARAM(ic_data->m_ops.set_chg_det, ic_data->drv_data, false);
	return ret;
}

int muic_platform_send_pdic_event(void *data, int event)
{
	struct muic_interface_t *muic_if;
	int ret = 0;

	if (!data) {
		pr_err("%s data is NULL\n", __func__);
		ret = -ENOENT;
		goto err;
	}

	muic_if = (struct muic_interface_t *)data;

	if (event >= EVENT_PDIC_MAX) {
		pr_err("%s pdic event is invalid. (%d)\n", __func__, event);
		ret = -EPERM;
		goto err;
	}

	pr_info("%s event=(%s)\n", __func__, pdic_event[event]);

	switch (event) {
	case EVENT_PDIC_RPRD:
		muic_platform_handle_pdic_rprd(muic_if);
		break;
	case EVENT_PDIC_ATTACH:
		muic_platform_handle_pdic_attach(muic_if);
		break;
	case EVENT_PDIC_DETACH:
		muic_platform_handle_pdic_detach(muic_if);
		break;
	case EVENT_PDIC_RID:
		muic_platform_handle_pdic_rid(muic_if);
		break;
	case EVENT_PDIC_WATER:
		muic_platform_handle_pdic_water(muic_if);
		break;
	case EVENT_PDIC_TA:
		muic_platform_handle_pdic_ta(muic_if);
		break;
	case EVENT_PDIC_OTG:
		muic_platform_handle_pdic_otg(muic_if);
		break;
	case EVENT_PDIC_ROLE_SWAP:
		muic_platform_handle_pdic_role_swap(muic_if);
		break;
	default:
		break;
	}
err:
	return ret;
}
EXPORT_SYMBOL_GPL(muic_platform_send_pdic_event);

void muic_platform_handle_vbus(struct muic_share_data *sdata,
		bool vbus_on)
{
	struct muic_interface_t *muic_if;

	if (!sdata) {
		pr_err("%s sdata is null\n", __func__);
		goto err;
	}

	muic_if = (struct muic_interface_t *)sdata;

	if (vbus_on) {
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
		if (muic_if->is_water) {
			pr_info("%s set hiccup path\n", __func__);
			muic_platform_set_hiccup(sdata, true);
		}
#endif
	} else
		;

	sdata->vbus_state = vbus_on;
err:
	return;
}
EXPORT_SYMBOL_GPL(muic_platform_handle_vbus);

void muic_platform_handle_rid(struct muic_share_data *sdata,
		int new_dev)
{
	struct muic_platform_data *pdata;

	pdata = sdata->pdata;

	pr_info("%s new_dev=%d\n", __func__, new_dev);

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	muic_send_lcd_on_uevent(pdata);
#endif
}
EXPORT_SYMBOL_GPL(muic_platform_handle_rid);

static int muic_platform_handle_detach_logically(struct muic_share_data *sdata,
	muic_attached_dev_t new_dev)
{
	int ret = 0;
	bool noti = true;
	bool force_path_open = true;

	switch (sdata->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		if (new_dev == ATTACHED_DEV_OTG_MUIC) {
			pr_info("%s: data role changed, not detach\n", __func__);
			force_path_open = false;
			goto out;
		}
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
		sdata->is_dcp_charger = false;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_UNKNOWN_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC ||
				new_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC ||
				new_dev == ATTACHED_DEV_JIG_UART_ON_MUIC ||
				new_dev == ATTACHED_DEV_JIG_UART_ON_VB_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_NONE_MUIC:
		force_path_open = false;
		goto out;
	default:
		pr_warn("%s try to attach without logically detach\n",
				__func__);
		goto out;
	}

	pr_info("%s attached(%d)!=new(%d), assume detach\n",
			__func__, sdata->attached_dev, new_dev);

	if (noti) {
		sdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
		if (!sdata->suspended)
			MUIC_SEND_NOTI_DETACH(sdata->attached_dev);
		else
			sdata->need_to_noti = true;
	}
out:
	if (force_path_open)
		muic_platform_set_switch_path(sdata, MUIC_PATH_OPEN);

	return ret;
}

int muic_platform_handle_attach(struct muic_share_data *sdata,
			muic_attached_dev_t new_dev)
{
	struct muic_interface_t *muic_if;
	int ret = 0;
	bool noti = true;
	struct muic_ic_data *ic_data;

	if (!sdata) {
		pr_info("%s sdata is NULL\n", __func__);
		goto out1;
	}
	mutex_lock(&sdata->attach_mutex);

	muic_if = (struct muic_interface_t *)sdata->muic_if;
	ic_data = (struct muic_ic_data *)sdata->ic_data;
	if (muic_if->is_water) {
		pr_info("%s water state. skip.\n", __func__);
		goto out;
	}

	pr_info("%s new_dev=(%d)\n", __func__, new_dev);

	if (new_dev == sdata->attached_dev) {
		pr_info("%s Duplicated(%d), just ignore\n",
				__func__, sdata->attached_dev);
		goto out;
	}

	sdata->is_jig_on = false;

	ret = muic_platform_handle_detach_logically(sdata, new_dev);

	switch (new_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		muic_platform_switch_usb_path(sdata);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		sdata->is_jig_on = true;
		muic_platform_switch_usb_path(sdata);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		sdata->is_jig_on = true;
		muic_platform_switch_uart_path(sdata);
		break;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	case ATTACHED_DEV_HICCUP_MUIC:
		sdata->pdata->uart_path = MUIC_PATH_UART_CP;
		muic_platform_switch_uart_path(sdata);
		break;
#endif
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
		sdata->is_dcp_charger = true;
#if IS_ENABLED(CONFIG_MUIC_HV)
		sdata->attached_dev = new_dev;
		MUIC_PDATA_FUNC(ic_data->m_ops.set_afc_ready, ic_data->drv_data, &ret);
#endif
		break;
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		muic_platform_switch_usb_path(sdata);
		break;
	default:
		pr_warn("%s unsupported dev(%d)\n", __func__,
				new_dev);
		ret = -ENODEV;
		goto out;
	}

	/* TODO: There is no needs to use JIGB pin by MUIC if PDIC is supported */
#if !IS_ENABLED(CONFIG_PDIC_SLSI_NON_MCU)
	MUIC_PDATA_FUNC(ic_data->m_ops.set_jig_ctrl_on, ic_data->drv_data, &ret);
#endif

	if (noti) {
		sdata->attached_dev = new_dev;
		if (!sdata->suspended)
			MUIC_SEND_NOTI_ATTACH(sdata->attached_dev);
		else
			sdata->need_to_noti = true;
	}

out:
	mutex_unlock(&sdata->attach_mutex);
out1:
	return ret;
}
EXPORT_SYMBOL_GPL(muic_platform_handle_attach);

int muic_platform_handle_detach(struct muic_share_data *sdata)
{
	int ret = 0;
	bool noti = true;
	bool force_path_open = true;
	muic_attached_dev_t attached_dev = sdata->attached_dev;
	struct muic_ic_data *ic_data;

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	pr_info("%s sdata->attached_dev=(%d)\n", __func__, sdata->attached_dev);

	sdata->is_dcp_charger = false;
	cancel_delayed_work(&sdata->hv_work);

	sdata->is_jig_on = false;

	if (sdata->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s Duplicated(%d), just ignore\n",
				__func__, sdata->attached_dev);
		goto out_without_noti;
	}

	switch (sdata->attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		break;
#if IS_ENABLED(CONFIG_MUIC_HV)
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	case ATTACHED_DEV_HICCUP_MUIC:
#endif
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
		MUIC_PDATA_FUNC(ic_data->m_ops.reset_hvcontrol_reg, ic_data->drv_data, &ret);
		break;
#endif
	case ATTACHED_DEV_UNOFFICIAL_ID_MUIC:
		goto out_without_noti;
	default:
		break;
	}

	if (noti) {
		sdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
		if (!sdata->suspended)
			MUIC_SEND_NOTI_DETACH(attached_dev);
		else
			sdata->need_to_noti = true;
	}

out_without_noti:
	if (force_path_open)
		muic_platform_set_switch_path(sdata, MUIC_PATH_OPEN);
	sdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
	muic_afc_request_cause_clear();

	return ret;
}
EXPORT_SYMBOL_GPL(muic_platform_handle_detach);

void muic_platform_handle_hv_attached_dev(struct muic_share_data *sdata,
				muic_attached_dev_t new_dev)
{
	int afc_disable = muic_platform_get_afc_disable(sdata);

	pr_info("%s sdata->attached_dev: %d, new_dev: %d, afc_disable: %d\n",
				__func__, sdata->attached_dev, new_dev, afc_disable);

	if (sdata->attached_dev == ATTACHED_DEV_HICCUP_MUIC)
		goto skip_noti;

	switch (new_dev) {
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		break;
	default:
		goto skip_noti;
	}

	if (new_dev == ATTACHED_DEV_AFC_CHARGER_5V_MUIC &&
		afc_disable) {
		pr_info("%s afc_disable is true => afc disabled type.\n", __func__);
		new_dev = ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC;
	}

	sdata->attached_dev = new_dev;
	MUIC_SEND_NOTI_ATTACH(sdata->attached_dev);

skip_noti:
	pr_info("%s -\n", __func__);
}
EXPORT_SYMBOL_GPL(muic_platform_handle_hv_attached_dev);

static void muic_platform_hv_work(struct work_struct *work)
{
	struct muic_share_data *sdata = container_of(work, struct muic_share_data, hv_work.work);
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	int ret = 0;

	pr_info("%s sdata->pdata->afc_disable = %d\n", __func__, sdata->pdata->afc_disable);
	mutex_lock(&sdata->hv_mutex);
	if (sdata->hv_voltage == HV_9V) /* send prepare type before 9v charging */
		muic_platform_handle_hv_attached_dev(sdata, ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC);

	MUIC_PDATA_FUNC(ic_data->m_ops.handle_hv_work, ic_data->drv_data, &ret);
	mutex_unlock(&sdata->hv_mutex);
}

static bool muic_platform_check_hv_ready(struct muic_share_data *sdata)
{
	bool ret = false;

	if (sdata->is_dcp_charger == false)
		pr_info("%s Not DCP charger, skip HV\n", __func__);
	else if (sdata->is_afc_pdic_ready == false)
		pr_info("%s Not PDIC ready, skip HV\n", __func__);
	else
		ret = true;

	return ret;
}

void muic_platform_send_hv_ready(struct muic_share_data *sdata)
{
	struct muic_platform_data *pdata = sdata->pdata;
	bool is_hv_ready = false;

	is_hv_ready = muic_platform_check_hv_ready(sdata);

	if (!is_hv_ready)
		goto err;

	sdata->hv_voltage = pdata->afc_disable ? HV_5V : HV_9V;

	pr_info("%s hv work start, hv_voltage=(%d)\n", __func__, sdata->hv_voltage);
	cancel_delayed_work(&sdata->hv_work);
	schedule_delayed_work(&sdata->hv_work, msecs_to_jiffies(sdata->hv_work_delay));
err:
	return;
}
EXPORT_SYMBOL_GPL(muic_platform_send_hv_ready);

#if IS_ENABLED(CONFIG_MUIC_HV)
static inline void muic_platform_hv_set_new_state(struct muic_share_data *sdata,
		muic_hv_state_t next_state)
{
	sdata->hv_state = next_state;
}

static inline muic_hv_state_t muic_platform_hv_get_current_state(struct muic_share_data *sdata)
{
	return sdata->hv_state;
}

bool muic_platform_hv_is_hv_dev(struct muic_share_data *sdata)
{
	bool ret = false;

	switch (sdata->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		ret = true;
		break;
	default:
		ret = false;
	}

	pr_info("%s attached_dev(%d)[%c]\n",
			__func__, sdata->attached_dev, (ret ? 'T' : 'F'));

	return ret;
}
EXPORT_SYMBOL_GPL(muic_platform_hv_is_hv_dev);

void muic_platform_hv_handle_state(struct muic_share_data *sdata,
		muic_hv_state_t next_state)
{
	struct muic_ic_data *ic_data;

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	muic_platform_hv_set_new_state(sdata, next_state);

	switch (next_state) {
	case HV_STATE_IDLE:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_reset, ic_data->drv_data);
		break;
	case HV_STATE_DCP_CHARGER:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_dcp_charger, ic_data->drv_data);
		break;
	case HV_STATE_FAST_CHARGE_ADAPTOR:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_fast_charge_adaptor, ic_data->drv_data);
		break;
	case HV_STATE_FAST_CHARGE_COMMUNICATION:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_fast_charge_communication, ic_data->drv_data);
		break;
	case HV_STATE_AFC_5V_CHARGER:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_afc_5v_charger, ic_data->drv_data);
		break;
	case HV_STATE_AFC_9V_CHARGER:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_afc_9v_charger, ic_data->drv_data);
		break;
	case HV_STATE_QC_CHARGER:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_qc_charger, ic_data->drv_data);
		break;
	case HV_STATE_QC_5V_CHARGER:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_qc_5v_charger, ic_data->drv_data);
		break;
	case HV_STATE_QC_9V_CHARGER:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_qc_9v_charger, ic_data->drv_data);
		break;
	case HV_STATE_QC_FAILED:
		MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_qc_failed, ic_data->drv_data);
		break;
	default:
		pr_err("%s not IS_ENABLED state : %d\n",
				__func__, next_state);
		break;
	}
}

bool muic_platform_get_pdic_cable_state(struct muic_share_data *sdata)
{
	bool ret = false;

	pr_info("%s call, attached_dev : %d\n", __func__, sdata->attached_dev);

	switch (sdata->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
		ret = true;
		break;
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
	case ATTACHED_DEV_USB_MUIC:
		/* prevent cable handling before pdic driver probe*/
		if (sdata->is_pdic_probe)
			ret = true;
		break;
#endif
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(muic_platform_get_pdic_cable_state);

int muic_platform_hv_state_manager(struct muic_share_data *sdata,
		muic_hv_transaction_t trans)
{
	muic_hv_state_t cur_state = muic_platform_hv_get_current_state(sdata);
	muic_hv_state_t next_state = HV_STATE_INVALID;
	bool skip_trans = false;

	switch (cur_state) {
	case HV_STATE_IDLE:
		switch (trans) {
		case HV_TRANS_DCP_DETECTED:
			next_state = HV_STATE_DCP_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_DCP_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VDNMON_LOW:
			next_state = HV_STATE_FAST_CHARGE_ADAPTOR;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_FAST_CHARGE_ADAPTOR:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_QC_CHARGER;
			break;
		case HV_TRANS_FAST_CHARGE_PING_RESPONSE:
			next_state = HV_STATE_FAST_CHARGE_COMMUNICATION;
			break;
		case HV_TRANS_FAST_CHARGE_REOPEN:
			next_state = HV_STATE_FAST_CHARGE_ADAPTOR;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_FAST_CHARGE_COMMUNICATION:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_QC_CHARGER;
			break;
		case HV_TRANS_FAST_CHARGE_PING_RESPONSE:
			next_state = HV_STATE_FAST_CHARGE_COMMUNICATION;
			break;
		case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_AFC_9V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_AFC_5V_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_AFC_9V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_AFC_9V_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VBUS_REDUCE:
			next_state = HV_STATE_AFC_5V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_QC_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_QC_9V_CHARGER;
			break;
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_QC_FAILED;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_QC_5V_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_QC_9V_CHARGER;
			break;
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_QC_FAILED;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_QC_9V_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VBUS_REDUCE:
			next_state = HV_STATE_QC_5V_CHARGER;
			break;
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_QC_FAILED;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_INVALID:
	case HV_STATE_MAX_NUM:
	default:
		skip_trans = true;
		break;
	}

	if (skip_trans) {
		pr_info("%s skip to transaction - state(%d), trans(%d)\n", __func__, cur_state, trans);
	} else {
		pr_info("%s state(%d->%d), trans(%d)\n", __func__, cur_state, next_state, trans);
		muic_platform_hv_handle_state(sdata, next_state);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(muic_platform_hv_state_manager);

void muic_platform_hv_init(struct muic_share_data *sdata)
{
	struct muic_ic_data *ic_data;

	ic_data = (struct muic_ic_data *)sdata->ic_data;
	sdata->hv_state = HV_STATE_IDLE;
	MUIC_PDATA_VOID_FUNC(ic_data->m_ops.hv_reset, ic_data->drv_data);
}
EXPORT_SYMBOL_GPL(muic_platform_hv_init);
#endif

#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
void hv_muic_change_afc_voltage(int tx_data)
{
	struct muic_share_data *sdata = mpl_data->sdata;
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;

	if (ic_data->m_ops.change_afc_voltage)
		ic_data->m_ops.change_afc_voltage(ic_data->drv_data, tx_data);
}
EXPORT_SYMBOL_GPL(hv_muic_change_afc_voltage);
#endif

static int muic_platform_afc_get_voltage(void)
{
	struct muic_share_data *sdata = mpl_data->sdata;
	struct muic_ic_data *ic_data;
	int ret = -ENODEV;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	if (ic_data->m_ops.afc_get_voltage)
		ret = ic_data->m_ops.afc_get_voltage(ic_data->drv_data);
err:
	return ret;

}

static int muic_platform_afc_set_vol(void *data, int vol)
{
	struct muic_share_data *sdata = data;
	struct muic_platform_data *pdata = NULL;
	int ret = 0;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	pdata = sdata->pdata;

	pr_info("%s sdata->attached_dev=(%d), voltage=(%dV)\n", __func__,
			sdata->attached_dev, vol);

	switch (sdata->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		if (vol != 9) {
			pr_info("%s same voltage. skip\n", __func__);
			ret = 0;
			goto err;
		} else {
			if (pdata->afc_disable == true) {
				pr_info("%s afc disable. skip\n", __func__);
				ret = 0;
				goto err;
			}
		}
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		if (vol != 5) {
			pr_info("%s same voltage. skip\n", __func__);
			ret = 0;
			goto err;
		}
		break;
	default:
		ret = 0;
		goto err;
	}
	sdata->hv_voltage = (vol == 5) ? HV_5V : HV_9V;

	cancel_delayed_work(&sdata->hv_work);
	schedule_delayed_work(&sdata->hv_work, msecs_to_jiffies(sdata->hv_work_delay));

	return 0;
err:
	return ret;
}

static int muic_platform_afc_set_voltage(int voltage)
{
	struct muic_share_data *sdata = mpl_data->sdata;
	int ret = 0;

	ret = muic_platform_afc_set_vol(sdata, voltage);

	return ret;
}

static int muic_platform_set_gpio_uart_sel(void *data, int uart_path)
{
	struct muic_share_data *sdata = data;
	struct muic_ic_data *ic_data;
	int ret = 0;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	MUIC_PDATA_FUNC_MULTI_PARAM(ic_data->m_ops.set_gpio_uart_sel,
		ic_data->drv_data, uart_path, &ret);
err:
	return ret;
}

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
static int muic_platform_set_uart_en(void *data, int en)
{
	struct muic_share_data *sdata = data;

	if (en == MUIC_ENABLE)
		sdata->is_rustproof = false;
	else
		sdata->is_rustproof = true;

	return 0;
}

static int muic_platform_get_usb_en(void *data)
{
	struct muic_share_data *sdata = data;
	int ret = 0;

	ret = sdata->attached_dev;

	return ret;
}

static int muic_platform_set_usb_en(void *data, int en)
{
	struct muic_share_data *sdata = data;
	int ret = 0;

	if (en == MUIC_ENABLE)
		ret = muic_platform_handle_attach(sdata, ATTACHED_DEV_USB_MUIC);
	else
		ret = muic_platform_handle_detach(sdata);

	return ret;
}

static int muic_platform_get_register(void *data, char *mesg)
{
	struct muic_share_data *sdata = data;
	struct muic_ic_data *ic_data;
	int ret = 0;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	MUIC_PDATA_FUNC_MULTI_PARAM(ic_data->m_ops.show_register,
		ic_data->drv_data, mesg, &ret);
err:
	return ret;
}

static int muic_platform_get_attached_dev(void *data)
{
	struct muic_share_data *sdata = data;
	int ret = 0;

	ret = sdata->attached_dev;

	return ret;
}

static void muic_platform_set_apo_factory(void *data)
{
	struct muic_share_data *sdata = data;

	sdata->is_factory_start = true;
}

static int muic_platform_get_vbus_value(void *data)
{
	struct muic_share_data *sdata = data;
	struct muic_ic_data *ic_data;
	int ret = 0;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	MUIC_PDATA_FUNC(ic_data->m_ops.get_vbus_value, ic_data->drv_data, &ret);
err:
	return ret;
}
#endif

int muic_platform_get_afc_disable(void *data)
{
	struct muic_share_data *sdata = data;
	struct muic_platform_data *pdata = sdata->pdata;
	int ret = 0;

	ret = (int)pdata->afc_disable;

	return ret;

}
EXPORT_SYMBOL(muic_platform_get_afc_disable);

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
static void muic_platform_set_afc_disable(void *data)
{
	struct muic_share_data *sdata = data;

	if (sdata == NULL)
		goto err;

	pr_info("%s sdata->attached_dev=(%d)\n", __func__, sdata->attached_dev);

	switch (sdata->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		muic_platform_send_hv_ready(sdata);
		break;
	default:
		break;
	}
err:
	return;

}
#endif

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static int muic_platform_set_overheat_hiccup(void *data, int en)
{
	struct muic_share_data *sdata = data;
	struct muic_ic_data *ic_data;
	int ret = 0;

	if (is_lpcharge_pdic_param())
		goto err;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	sdata->is_hiccup_mode = en;
	ic_data = (struct muic_ic_data *)sdata->ic_data;

	MUIC_PDATA_FUNC_MULTI_PARAM(ic_data->m_ops.set_hiccup_mode,
		ic_data->drv_data, en, &ret);
err:
	return ret;
}

static int muic_platform_set_hiccup(void *data, int en)
{
	struct muic_share_data *sdata = data;
	struct muic_ic_data *ic_data;
	int ret = 0;

	if (is_lpcharge_pdic_param())
		goto err;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	sdata->is_hiccup_mode = en;
	ic_data = (struct muic_ic_data *)sdata->ic_data;

	MUIC_PDATA_FUNC_MULTI_PARAM(ic_data->m_ops.set_hiccup,
		ic_data->drv_data, en, &ret);
err:
	return ret;
}

static int muic_platform_get_hiccup(void *data)
{
	struct muic_share_data *sdata = data;
	struct muic_ic_data *ic_data;
	int ret = 0;

	if (is_lpcharge_pdic_param())
		goto err;

	if (sdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ic_data = (struct muic_ic_data *)sdata->ic_data;

	MUIC_PDATA_FUNC(ic_data->m_ops.get_hiccup_mode, ic_data->drv_data, &ret);
err:
	return ret;
}

static int muic_platform_set_hiccup_mode_cb(int en)
{
	struct muic_share_data *sdata;
	int ret = 0;

	if (!mpl_data || !mpl_data->sdata) {
		ret = -ENOMEM;
		goto err;
	}

	sdata = mpl_data->sdata;

	ret = muic_platform_set_overheat_hiccup(sdata, en);
err:
	return ret;
}
#endif

int muic_platform_init_gpio_cb(struct muic_share_data *sdata)
{
	struct muic_platform_data *pdata = sdata->pdata;
	int ret = 0;

#if IS_MODULE(CONFIG_MUIC_NOTIFIER)
	if (pdata->init_gpio_cb)
		ret = pdata->init_gpio_cb(get_switch_sel());
#else
	if (pdata->init_gpio_cb)
		ret = pdata->init_gpio_cb();
#endif

	if (ret) {
		pr_err("%s failed to init gpio(%d)\n", __func__, ret);
		goto err;
	}

	pr_info("%s: usb_path(%d), uart_path(%d)\n", __func__,
		pdata->usb_path, pdata->uart_path);
err:
	return ret;
}
EXPORT_SYMBOL_GPL(muic_platform_init_gpio_cb);

int muic_platform_init_switch_dev(struct muic_share_data *sdata)
{
	struct muic_platform_data *pdata = sdata->pdata;
	int ret = 0;

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
	ret = muic_sysfs_init(pdata);
	if (ret) {
		pr_err("failed to create sysfs\n");
		goto err;
	}
#endif

	if (pdata->init_switch_dev_cb)
		pdata->init_switch_dev_cb();

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
err:
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(muic_platform_init_switch_dev);

void muic_platform_deinit_switch_dev(struct muic_share_data *sdata)
{
#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
	struct muic_platform_data *pdata = sdata->pdata;

	muic_sysfs_deinit(pdata);
#endif
}
EXPORT_SYMBOL_GPL(muic_platform_deinit_switch_dev);

#ifdef CONFIG_MUIC_COMMON_SYSFS
static void fill_muic_sysfs_cb(struct muic_platform_data *pdata)
{
	pdata->sysfs_cb.set_uart_en = muic_platform_set_uart_en;
	pdata->sysfs_cb.get_usb_en = muic_platform_get_usb_en;
	pdata->sysfs_cb.set_usb_en = muic_platform_set_usb_en;
	pdata->sysfs_cb.get_adc = muic_platform_get_adc;
	pdata->sysfs_cb.get_register = muic_platform_get_register;
	pdata->sysfs_cb.get_attached_dev = muic_platform_get_attached_dev;
	pdata->sysfs_cb.set_apo_factory = muic_platform_set_apo_factory;
	pdata->sysfs_cb.set_afc_disable = muic_platform_set_afc_disable;
	pdata->sysfs_cb.afc_set_voltage = muic_platform_afc_set_vol;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	pdata->sysfs_cb.set_overheat_hiccup = muic_platform_set_overheat_hiccup;
	pdata->sysfs_cb.set_hiccup = muic_platform_set_hiccup;
	pdata->sysfs_cb.get_hiccup = muic_platform_get_hiccup;
#endif
	pdata->sysfs_cb.get_vbus_value = muic_platform_get_vbus_value;
}
#endif

int mpl_init(void)
{
	int ret = 0;

	mpl_data = kzalloc(sizeof(struct m_p_l_data), GFP_KERNEL);
	if (unlikely(!mpl_data)) {
		pr_err("%s mpl data alloc fail\n", __func__);
		ret = -ENOMEM;
	}

	return ret;
}
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
static enum power_supply_property muic_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *muic_supplied_to[] = {
	"battery",
};

static int muic_manager_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct muic_share_data *sdata = power_supply_get_drvdata(psy);
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property)psp;
	int ret;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_LSI_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
		case POWER_SUPPLY_LSI_PROP_PM_VCHGIN:
			MUIC_PDATA_FUNC(ic_data->m_ops.get_vbus_state, ic_data->drv_data, &ret);
			val->intval = ret;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int muic_manager_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct muic_share_data *sdata = power_supply_get_drvdata(psy);
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	int ret;
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_LSI_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
		case POWER_SUPPLY_LSI_PROP_WATER_STATUS:
			MUIC_PDATA_VOID_FUNC_MULTI_PARAM(ic_data->m_ops.set_water_state, ic_data->drv_data, val->intval);
			break;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
		case POWER_SUPPLY_LSI_PROP_HICCUP_MODE:
			MUIC_PDATA_VOID_FUNC_MULTI_PARAM(ic_data->m_ops.set_hiccup, ic_data->drv_data, val->intval);
			break;
#endif
		case POWER_SUPPLY_LSI_PROP_PM_VCHGIN:
			MUIC_PDATA_FUNC_MULTI_PARAM(ic_data->m_ops.pm_chgin_irq,
				ic_data->drv_data, val->intval, &ret);
			break;
		case POWER_SUPPLY_LSI_PROP_PM_FACTORY:
			sdata->is_bypass = true;
			MUIC_PDATA_VOID_FUNC(ic_data->m_ops.set_bypass, ic_data->drv_data);
			break;
		case POWER_SUPPLY_LSI_PROP_PD_SUPPORT:
			sdata->is_pdic_probe = true;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int muic_platform_psy_init(struct muic_share_data *sdata, struct device *parent)
{
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	if (sdata == NULL || parent == NULL) {
		pr_err("%s NULL data\n", __func__);
		return -1;
	}

	sdata->psy_muic_desc.name           = "muic-manager";
	sdata->psy_muic_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	sdata->psy_muic_desc.get_property   = muic_manager_get_property;
	sdata->psy_muic_desc.set_property   = muic_manager_set_property;
	sdata->psy_muic_desc.properties     = muic_props;
	sdata->psy_muic_desc.num_properties = ARRAY_SIZE(muic_props);

	psy_cfg.drv_data = sdata;
	psy_cfg.supplied_to = muic_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(muic_supplied_to);

	sdata->psy_muic = power_supply_register(parent, &sdata->psy_muic_desc, &psy_cfg);
	if (IS_ERR(sdata->psy_muic)) {
		ret = (int)PTR_ERR(sdata->psy_muic);
		pr_err("%s: Failed to Register psy_muic, ret : %d\n", __func__, ret);
	}
	return ret;
}
EXPORT_SYMBOL(muic_platform_psy_init);
#endif

int register_muic_platform_layer(struct muic_share_data *sdata)
{
	struct muic_platform_data *pdata;
	struct muic_interface_t *muic_if;
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	struct muic_dev *muic_d;
#endif
	int ret = 0;

	pr_info("%s +\n", __func__);
	if (!mpl_data) {
		ret = mpl_init();
		if (ret < 0)
			goto err;
	}

	mpl_data->sdata = sdata;
	mpl_data->pdata = &muic_pdata;

	pdata = mpl_data->pdata;

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	muic_d = kzalloc(sizeof(struct muic_dev), GFP_KERNEL);
	if (!muic_d) {
		ret = -ENOMEM;
		goto err;
	}
	muic_d->ops = NULL;
	muic_d->data = (void *)pdata;
	pdata->man = register_muic(muic_d);
#endif

	sdata->pdata = pdata;
	pdata->drv_data = sdata;

	sdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
	sdata->is_rustproof = false;
	sdata->is_factory_start = false;
	sdata->is_dcp_charger = false;
	sdata->is_afc_pdic_ready = false;
	sdata->is_afc_water_disable = false;
	sdata->is_pdic_attached = false;
	sdata->vbus_state = MPL_VBUS_LOW;
	sdata->is_bypass = false;
	sdata->is_pdic_probe = false;

	mutex_init(&sdata->hv_mutex);
	mutex_init(&sdata->attach_mutex);

	pdata->muic_afc_get_voltage_cb = muic_platform_afc_get_voltage;
	pdata->muic_afc_set_voltage_cb = muic_platform_afc_set_voltage;
	pdata->set_gpio_uart_sel = muic_platform_set_gpio_uart_sel;
#if defined(CONFIG_HICCUP_CHARGER)
	sdata->is_hiccup_mode = false;
	pdata->muic_set_hiccup_mode_cb =
				muic_platform_set_hiccup_mode_cb;
#endif
	INIT_DELAYED_WORK(&sdata->hv_work, muic_platform_hv_work);

	muic_if = register_muic_platform_manager(sdata);
	if (!muic_if) {
		pr_err("%s failed to init muic manager\n", __func__);
		ret = -ENOMEM;
		goto err_kfree1;
	}
	sdata->muic_if = muic_if;
#ifdef CONFIG_MUIC_COMMON_SYSFS
	fill_muic_sysfs_cb(pdata);
#endif
	pr_info("%s -\n", __func__);
	return ret;

err_kfree1:
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	kfree(muic_d);
#endif
err:
	pr_info("%s -\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(register_muic_platform_layer);

void unregister_muic_platform_layer(struct muic_share_data *sdata)
{
	if (mpl_data && mpl_data->sdata == sdata)
		mpl_data->sdata = NULL;

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
	power_supply_unregister(sdata->psy_muic);
#endif
	mutex_destroy(&sdata->hv_mutex);
	unregister_muic_platform_manager(sdata);
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	kfree(sdata->pdata->man->muic_d);
#endif
}
EXPORT_SYMBOL_GPL(unregister_muic_platform_layer);

static int __init muic_platform_layer_init(void)
{
	int ret = 0;

	if (!mpl_data)
		ret = mpl_init();

	return ret;
}

static void __exit muic_platform_layer_exit(void)
{
	kfree(mpl_data);
	mpl_data = NULL;
}

module_init(muic_platform_layer_init);
module_exit(muic_platform_layer_exit);

MODULE_AUTHOR("Samsung USB Team");
MODULE_DESCRIPTION("muic platform layer");
MODULE_LICENSE("GPL");
