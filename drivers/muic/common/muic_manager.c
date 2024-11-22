/*
 * muic_manager.c
 *
 * Copyright (C) 2019 Samsung Electronics
 * Sejong Park <sejong123.park@samsung.com>
 * Taejung Kim <tj.kim@samsung.com>
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>
#include <linux/string.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#if IS_ENABLED(CONFIG_MFD_S2MU106)
#include "linux/mfd/slsi/s2mu106/s2mu106.h"
#endif /* CONFIG_MFD_S2MU106 */
#include <linux/muic/common/muic_interface.h>
#if defined(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
#include "../../battery/charger/s2mu106_charger/s2mu106_pmeter.h"
#endif

#define MUIC_PDIC_NOTI_ATTACH (1)
#define MUIC_PDIC_NOTI_DETACH (-1)
#define MUIC_PDIC_NOTI_UNDEFINED (0)

static struct pdic_rid_desc_t pdic_rid_tbl[] = {
	[PDIC_RID_UNDEFINED] = {"UNDEFINED", ATTACHED_DEV_NONE_MUIC},
	[PDIC_RID_000K] = {"000K", ATTACHED_DEV_OTG_MUIC},
	[PDIC_RID_001K] = {"001K", ATTACHED_DEV_MHL_MUIC},
	[PDIC_RID_255K] = {"255K", ATTACHED_DEV_JIG_USB_OFF_MUIC},
	[PDIC_RID_301K] = {"301K", ATTACHED_DEV_JIG_USB_ON_MUIC},
	[PDIC_RID_523K] = {"523K", ATTACHED_DEV_JIG_UART_OFF_MUIC},
	[PDIC_RID_619K] = {"619K", ATTACHED_DEV_JIG_UART_ON_MUIC},
	[PDIC_RID_OPEN] = {"OPEN", ATTACHED_DEV_NONE_MUIC},
};

static void _muic_manager_switch_uart_path(struct muic_interface_t *muic_if, int path)
{
	if (muic_if->pdata->gpio_uart_sel)
		muic_if->set_gpio_uart_sel(muic_if->muic_data, path);

	if (muic_if->set_switch_to_uart)
		muic_if->set_switch_to_uart(muic_if->muic_data);
	else
		pr_err("%s:function not set!\n", __func__);
}

static void _muic_manager_switch_usb_path(struct muic_interface_t *muic_if, int path)
{
	if (muic_if->pdata->gpio_usb_sel)
		muic_if->set_gpio_usb_sel(muic_if->muic_data, path);

	if (muic_if->set_switch_to_usb)
		muic_if->set_switch_to_usb(muic_if->muic_data);
	else
		pr_err("%s:function not set!\n", __func__);
}

static int muic_manager_switch_path(struct muic_interface_t *muic_if, int path)
{
	switch (path) {
	case MUIC_PATH_OPEN:
		if (muic_if->set_com_to_open)
			muic_if->set_com_to_open(muic_if->muic_data);
		break;

	case MUIC_PATH_USB_AP:
	case MUIC_PATH_USB_CP:
		_muic_manager_switch_usb_path(muic_if, muic_if->pdata->usb_path);
		break;
	case MUIC_PATH_UART_AP:
	case MUIC_PATH_UART_CP:
		_muic_manager_switch_uart_path(muic_if, muic_if->pdata->uart_path);
		break;

	default:
		pr_err("%s:A wrong com path!\n", __func__);
		return -1;
	}

	return 0;
}

static int muic_manager_get_vbus(struct muic_interface_t *muic_if)
{
	int ret = 0;

	if (muic_if->get_vbus)
		ret = muic_if->get_vbus(muic_if->muic_data);

	return ret;
}

static bool muic_manager_is_supported_dev(int attached_dev)
{
	switch (attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		return true;
	default:
		break;
	}

	return false;
}

int muic_manager_is_pdic_supported_dev(muic_attached_dev_t new_dev)
{
	switch (new_dev) {
	/* Legacy TA/USB. Noti. will be sent when ATTACH is received from PDIC. */
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		return 1;
	default:
		break;
	}

	return 0;
}

void muic_manager_handle_pdic_detach_always(struct muic_interface_t *muic_if)
{
	pr_info("%s\n", __func__);
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
	if (muic_if->set_chg_det)
		muic_if->set_chg_det(muic_if->muic_data, true);
#endif
#if IS_ENABLED(CONFIG_HV_MUIC_S2MU004_AFC) || IS_ENABLED(CONFIG_MUIC_HV)
	muic_if->is_afc_pdic_ready = false;
#endif
	muic_if->is_pdic_attached = false;
}

void muic_manager_handle_pdic_detach(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
	struct muic_platform_data *pdata = muic_if->pdata;
	int vbus = muic_manager_get_vbus(muic_if);

	pr_info("%s\n", __func__);

	if (pdic->pdic_evt_rprd) {
		/* FIXME : pvendor
		* if (pvendor && pvendor->enable_chgdet)
		* pvendor->enable_chgdet(muic_if->regmapdic, 1);
		*/
	}

	if (vbus == false) {
		if (muic_if->set_cable_state)
			muic_if->set_cable_state(muic_if->muic_data, ATTACHED_DEV_NONE_MUIC);
	}

	if (pdata->jig_uart_cb)
		pdata->jig_uart_cb(0);

	/* Reset status & flags */
	pdic->attached_dev = 0;
	pdic->pdic_evt_rid = 0;
	pdic->pdic_evt_rprd = 0;
	pdic->pdic_evt_roleswap = 0;
	pdic->pdic_evt_attached = MUIC_PDIC_NOTI_UNDEFINED;

	muic_if->legacy_dev = 0;
	muic_if->attached_dev = 0;
#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
	pdic->pdic_evt_dcdcnt = 0;
	muic_if->is_dcdtmr_intr = false;
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
	muic_if->prswap_status = MUIC_PRSWAP_UNDIFINED;
#endif
}

#ifndef CONFIG_SEC_FACTORY
void muic_manager_handle_pdic_rid_open(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
	struct muic_platform_data *pdata = muic_if->pdata;

	pr_info("%s\n", __func__);

	if (muic_if->set_cable_state)
		muic_if->set_cable_state(muic_if->muic_data, ATTACHED_DEV_NONE_MUIC);

	muic_manager_switch_path(muic_if, MUIC_PATH_OPEN);
	if (muic_manager_is_supported_dev(pdic->attached_dev))
		MUIC_SEND_NOTI_DETACH(pdic->attached_dev);
	else if (muic_if->legacy_dev != ATTACHED_DEV_NONE_MUIC)
		MUIC_SEND_NOTI_DETACH(muic_if->legacy_dev);

	if (pdata->jig_uart_cb)
		pdata->jig_uart_cb(0);

	muic_if->legacy_dev = 0;
	muic_if->attached_dev = 0;
#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
	muic_if->is_dcdtmr_intr = false;
#endif
}
#endif

void muic_manager_set_legacy_dev(struct muic_interface_t *muic_if, int new_dev)
{
	pr_info("%s: %d->%d\n", __func__, muic_if->legacy_dev, new_dev);

	muic_if->legacy_dev = new_dev;
}
EXPORT_SYMBOL(muic_manager_set_legacy_dev);

/* Get the charger type from muic interrupt or by reading the register directly */
int muic_manager_get_legacy_dev(struct muic_interface_t *muic_if)
{
	return muic_if->legacy_dev;
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static void muic_manager_show_status(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;

	pr_info("%s: attached_dev:%d rid:%d rprd:%d attached:%d legacy_dev:%d\n", __func__,
			pdic->attached_dev, pdic->pdic_evt_rid, pdic->pdic_evt_rprd,
			pdic->pdic_evt_attached, muic_if->legacy_dev);
}
#endif

#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
int muic_manager_dcd_rescan(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
	int vbus = muic_manager_get_vbus(muic_if);

	pr_info("%s : pdic_evt_attached(%d), is_dcdtmr_intr(%d), pdic_evt_dcdcnt(%d)\n",
			__func__, pdic->pdic_evt_attached, muic_if->is_dcdtmr_intr, pdic->pdic_evt_dcdcnt);

	if (!(muic_if->opmode & OPMODE_PDIC)) {
		pr_info("%s : it's SMD board, skip rescan", __func__);
		goto SKIP_RESCAN;
	}

	/* W/A for Incomplete insertion case */
	if (muic_if->is_dcdtmr_intr && vbus && pdic->pdic_evt_dcdcnt < 1) {
		pr_info("%s: Incomplete insertion. Do chgdet again\n", __func__);
		pdic->pdic_evt_dcdcnt++;

		if (muic_if->set_dcd_rescan != NULL)
			muic_if->set_dcd_rescan(muic_if->muic_data);

		return 0;
	}

SKIP_RESCAN:
	return 1;
}
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static int muic_manager_handle_legacy_dev(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
	int attached_dev = 0;

	pr_info("%s: vbvolt:%d legacy_dev:%d\n", __func__,
			muic_if->vps.t.vbvolt, muic_if->legacy_dev);

	/* 1. Run a charger detection algorithm manually if necessary. */
	msleep(200);

	/* 2. Get the result by polling or via an interrupt */
	attached_dev = muic_manager_get_legacy_dev(muic_if);
	pr_info("%s: detected legacy_dev=%d\n", __func__, attached_dev);

	/* 3. Noti. if supported. */
	if (!muic_manager_is_pdic_supported_dev(attached_dev)) {
		pr_info("%s: Unsupported legacy_dev=%d\n", __func__, attached_dev);
		return 0;
	}

	if (muic_manager_is_supported_dev(pdic->attached_dev)) {
		MUIC_SEND_NOTI_DETACH(pdic->attached_dev);
		pdic->attached_dev = 0;
	} else if (muic_if->legacy_dev != ATTACHED_DEV_NONE_MUIC) {
		MUIC_SEND_NOTI_DETACH(muic_if->legacy_dev);
		muic_if->legacy_dev = 0;
	}

	pdic->attached_dev = attached_dev;
	MUIC_SEND_NOTI_ATTACH(attached_dev);

	return 0;
}
#endif

void muic_manager_init_dev_desc(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;

	pr_info("%s\n", __func__);
	pdic->attached_dev = 0;
	pdic->pdic_evt_rid = 0;
	pdic->pdic_evt_rprd = 0;
	pdic->pdic_evt_roleswap = 0;
	pdic->pdic_evt_attached = MUIC_PDIC_NOTI_UNDEFINED;
#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
	pdic->pdic_evt_dcdcnt = 0;
#endif
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static int muic_manager_conv_rid_to_dev(struct muic_interface_t *muic_if, int rid, int vbus)
{
	int attached_dev = 0;

	pr_info("%s rid=%d vbus=%d\n", __func__, rid, vbus);

	if (rid < 0 || rid >  PDIC_RID_OPEN) {
		pr_err("%s:Out of RID range: %d\n", __func__, rid);
		return 0;
	}

	if ((rid == PDIC_RID_619K) && vbus) {
		attached_dev = ATTACHED_DEV_JIG_UART_ON_VB_MUIC;
	} else if ((rid == PDIC_RID_523K) && vbus) {
		attached_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
	} else
		attached_dev = muic_if->pdic->rid_desc[rid].attached_dev;

	return attached_dev;
}

static bool muic_manager_is_valid_rid_open(struct muic_interface_t *muic_if, int vbus)
{
	int i, retry = 5;

	if (vbus)
		return true;

	for (i = 0; i < retry; i++) {
		pr_info("%s: %dth ...\n", __func__, i);
		msleep(20);
		if (muic_manager_get_vbus(muic_if))
			return 1;
	}

	return 0;
}

static int muic_manager_handle_pdic_attach(struct muic_interface_t *muic_if, void *data)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
#endif
	struct muic_platform_data *pdata = muic_if->pdata;
	int vbus = muic_manager_get_vbus(muic_if);

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	pdic->pdic_evt_attached = pnoti->attach ?
		MUIC_PDIC_NOTI_ATTACH : MUIC_PDIC_NOTI_DETACH;

	/* Attached */
	if (pdic->pdic_evt_attached == MUIC_PDIC_NOTI_ATTACH) {
		pr_info("%s: Attach\n", __func__);
		muic_if->is_pdic_attached = true;

		if (pdic->pdic_evt_roleswap) {
			pr_info("%s: roleswap event, attach USB\n", __func__);
			pdic->pdic_evt_roleswap = 0;
			if (muic_manager_get_vbus(muic_if)) {
				pdic->attached_dev = ATTACHED_DEV_USB_MUIC;
				if (muic_if->set_cable_state)
					muic_if->set_cable_state(muic_if->muic_data, pdic->attached_dev);
				muic_manager_switch_path(muic_if, MUIC_PATH_USB_AP);
				MUIC_SEND_NOTI_ATTACH(pdic->attached_dev);
			}
			return 0;
		}

		if (pnoti->rprd) {
			pr_info("%s: RPRD\n", __func__);
			pdic->pdic_evt_rprd = 1;
			pdic->attached_dev = ATTACHED_DEV_OTG_MUIC;
#if IS_ENABLED(CONFIG_MUIC_HV)
			if (muic_if->hv_reset)
				muic_if->hv_reset(muic_if->muic_data);
#endif
			if (muic_if->set_cable_state)
				muic_if->set_cable_state(muic_if->muic_data, pdic->attached_dev);
			muic_manager_switch_path(muic_if, MUIC_PATH_USB_AP);
			return 0;
		}
#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
#if IS_ENABLED(CONFIG_HV_MUIC_S2MU004_AFC)
		if (muic_if->is_afc_reset) {
			pr_info("%s: DCD RESCAN after afc reset\n", __func__);
			muic_if->is_afc_reset = false;
			if (muic_if->set_dcd_rescan != NULL && !muic_if->is_dcp_charger)
				muic_if->set_dcd_rescan(muic_if->muic_data);
		}
#endif
#endif
		if (muic_manager_is_valid_rid_open(muic_if, vbus))
			pr_info("%s: Valid VBUS-> handled in irq handler\n", __func__);
		else
			pr_info("%s: No VBUS-> doing nothing.\n", __func__);

		/* PDIC ATTACH means NO WATER */
		if (muic_if->afc_water_disable) {
			pr_info("%s: Water is not detected, AFC Enable\n", __func__);
			muic_if->afc_water_disable = false;
		}

#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
		/* W/A for Incomplete insertion case */
		pdic->pdic_evt_dcdcnt = 0;
		if (muic_if->is_dcdtmr_intr && vbus) {
			if (muic_if->vps.t.chgdetrun) {
				pr_info("%s: Incomplete insertion. Chgdet runnung\n", __func__);
				return 0;
			}
			pr_info("%s: Incomplete insertion. Do chgdet again\n", __func__);
			muic_if->is_dcdtmr_intr = false;

			if (muic_if->set_dcd_rescan != NULL)
				muic_if->set_dcd_rescan(muic_if->muic_data);
		}
#endif
	} else {
		muic_manager_handle_pdic_detach_always(muic_if);
		if (pnoti->rprd) {
			/* Role swap detach: attached=0, rprd=1 */
			pr_info("%s: role swap event\n", __func__);
			pdic->pdic_evt_roleswap = 1;
		} else if (vbus && !muic_core_get_pdic_cable_state(pdata)) {
			pr_info("%s: Valid VBUS, return\n", __func__);
#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
				if (muic_if->is_dcp_charger) {
					pr_info("%s: reset afc\n", __func__);
#if IS_ENABLED(CONFIG_HV_MUIC_S2MU004_AFC) || IS_ENABLED(CONFIG_MUIC_HV)
				if (muic_if->set_afc_reset)
					muic_if->set_afc_reset(muic_if->muic_data);
#endif
			}
#endif
		} else {
			/* Detached */
			muic_manager_handle_pdic_detach(muic_if);
		}
	}

	return 0;
}

static int muic_manager_handle_pdic_factory_jig(struct muic_interface_t *muic_if,
		int rid, int prev_rid, int vbus)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
	struct muic_platform_data *pdata = muic_if->pdata;
	int attached_dev = 0;

	pr_info("%s: rid:%d prev_rid:%d vbus:%d\n", __func__, rid, prev_rid, vbus);

	if (prev_rid && rid != prev_rid)
		if (pdata->jig_uart_cb)
			pdata->jig_uart_cb(0);

	switch (rid) {
	case PDIC_RID_255K:
	case PDIC_RID_301K:
		if (pdata->jig_uart_cb)
			pdata->jig_uart_cb(1);
		muic_manager_switch_path(muic_if, MUIC_PATH_USB_AP);
		break;
	case PDIC_RID_523K:
	case PDIC_RID_619K:
		if (pdata->jig_uart_cb)
			pdata->jig_uart_cb(1);
		muic_manager_switch_path(muic_if, MUIC_PATH_UART_AP);
		break;
	default:
		pr_info("%s: Unsupported rid\n", __func__);
		return 0;
	}

	attached_dev = muic_manager_conv_rid_to_dev(muic_if, rid, vbus);

	if (attached_dev != pdic->attached_dev) {
		if (muic_manager_is_supported_dev(pdic->attached_dev)) {
			pdic->attached_dev = 0;
		} else if (muic_if->legacy_dev != ATTACHED_DEV_NONE_MUIC) {
			muic_if->legacy_dev = 0;
		}

		pdic->attached_dev = attached_dev;
#if IS_ENABLED(CONFIG_MUIC_HV)
		if (muic_if->hv_reset)
			muic_if->hv_reset(muic_if->muic_data);
#endif
		if (muic_if->set_cable_state)
			muic_if->set_cable_state(muic_if->muic_data, pdic->attached_dev);
		MUIC_SEND_NOTI_ATTACH(attached_dev);
	}

	return 0;
}

static int muic_manager_handle_pdic_rid(struct muic_interface_t *muic_if, void *data)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
	struct muic_platform_data *pdata = muic_if->pdata;
	int rid, vbus, prev_rid;
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_RID_TYPEDEF *pnoti = (PD_NOTI_RID_TYPEDEF *)data;
#endif

	pr_info("%s: src:%d dest:%d id:%d rid:%d sub2:%d sub3:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->rid, pnoti->sub2, pnoti->sub3);

	rid = pnoti->rid;

	if (rid > PDIC_RID_OPEN) {
		pr_info("%s: Out of range of RID\n", __func__);
		return 0;
	}

	if (pdic->pdic_evt_attached != MUIC_PDIC_NOTI_ATTACH) {
		pr_info("%s: RID but No ATTACH->discarded\n", __func__);
		return 0;
	}

	prev_rid = pdic->pdic_evt_rid;
	pdic->pdic_evt_rid = rid;

	switch (rid) {
	case PDIC_RID_000K:
		pr_info("%s: OTG -> RID000K\n", __func__);
		muic_manager_switch_path(muic_if, MUIC_PATH_USB_AP);
		vbus = muic_manager_get_vbus(muic_if);
		pdic->attached_dev = muic_manager_conv_rid_to_dev(muic_if, rid, vbus);
		return 0;
	case PDIC_RID_001K:
		pr_info("%s: MHL -> discarded.\n", __func__);
		return 0;
	case PDIC_RID_255K:
	case PDIC_RID_301K:
	case PDIC_RID_523K:
	case PDIC_RID_619K:
		vbus = muic_manager_get_vbus(muic_if);
		if (muic_if->set_jig_state)
			muic_if->set_jig_state(muic_if->muic_data, true);
		muic_manager_handle_pdic_factory_jig(muic_if, rid, prev_rid, vbus);
		break;
	case PDIC_RID_OPEN:
	case PDIC_RID_UNDEFINED:
		vbus = muic_manager_get_vbus(muic_if);
		if (pdic->pdic_evt_attached == MUIC_PDIC_NOTI_ATTACH &&
			muic_manager_is_valid_rid_open(muic_if, vbus)) {
			if (pdata->jig_uart_cb)
				pdata->jig_uart_cb(0);
			/*
			 * USB team's requirement.
			 * Set AP USB for enumerations.
			 */
			muic_manager_switch_path(muic_if, MUIC_PATH_USB_AP);
			muic_manager_handle_legacy_dev(muic_if);
		} else {
			/* RID OPEN + No VBUS = Assume detach */
#ifndef CONFIG_SEC_FACTORY
			muic_manager_handle_pdic_rid_open(muic_if);
#else
			muic_manager_handle_pdic_detach(muic_if);
#endif
		}
		if (muic_if->set_jig_state)
			muic_if->set_jig_state(muic_if->muic_data, false);
		break;
	default:
		pr_err("%s:Undefined RID\n", __func__);
		return 0;
	}

	return 0;
}

static int muic_manager_handle_pdic_water(struct muic_interface_t *muic_if, void *data)
{
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
#endif

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	muic_if->afc_water_disable = pnoti->attach ? true:false;
	if (muic_if->set_water_detect)
		muic_if->set_water_detect(muic_if->muic_data, muic_if->afc_water_disable);

	pr_info("%s: Water detect : %s\n", __func__, pnoti->attach ? "en":"dis");

	return 0;
}

#ifndef CONFIG_SEC_FACTORY
static int muic_manager_handle_pdic_water_from_boot(struct muic_interface_t *muic_if, void *data)
{
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
#endif

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	if (muic_if->set_water_detect_from_boot)
		muic_if->set_water_detect_from_boot(muic_if->muic_data, true);
	return 0;
}

#if IS_ENABLED(CONFIG_HV_MUIC_S2MU004_AFC) || IS_ENABLED(CONFIG_MUIC_HV)
static int muic_manager_handle_pdic_TA(struct muic_interface_t *muic_if, void *data)
{
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
#endif

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	muic_if->is_afc_pdic_ready = true;
	if (muic_if->set_afc_ready)
		muic_if->set_afc_ready(muic_if->muic_data, muic_if->is_afc_pdic_ready);

	return 0;
}
#endif
#endif

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
static int muic_manager_handle_pdic_role_swap(struct muic_interface_t *muic_if, void *data)
{
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;
	struct pdic_desc_t *pdic = muic_if->pdic;

	if (pdic->pdic_evt_attached != MUIC_PDIC_NOTI_ATTACH)
		return 0;

	pr_info("%s: src:%d dest:%d sub1:%d\n", __func__, pnoti->src, pnoti->dest,
		pnoti->sub1);

	if (pnoti->sub1 == true) {
		/* sink -> src */
		muic_if->prswap_status = MUIC_PRSWAP_TO_SRC;
	} else {
		/* src -> sink */
		muic_if->prswap_status = MUIC_PRSWAP_TO_SINK;
	}

	if (muic_if->set_chg_det)
		muic_if->set_chg_det(muic_if->muic_data, false);

	return 0;
}
#endif

static int muic_manager_handle_otg(struct muic_interface_t *muic_if, void *data)
{
	int ret = 0;
	struct pdic_desc_t *pdic = muic_if->pdic;
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;
#endif

	pr_info("%s sub1:%d, check_usb_killer:%p, set_cable_state:%p\n", __func__,
		pnoti->sub1, muic_if->check_usb_killer, muic_if->set_cable_state);

	if (pnoti->sub1 == false || muic_if->check_usb_killer == false
			|| muic_if->set_cable_state == false) {
		return 0;
	}

	muic_if->is_pdic_attached = true;
	/* OTG Attach*/
	ret = muic_if->check_usb_killer(muic_if->muic_data);
	if (ret == MUIC_NORMAL_OTG) {
		pdic->attached_dev = ATTACHED_DEV_OTG_MUIC;
		muic_if->set_cable_state(muic_if->muic_data, pdic->attached_dev);
		pdic->pdic_evt_rprd = 1;
		MUIC_SEND_NOTI_ATTACH(ATTACHED_DEV_OTG_MUIC);
		MUIC_SEND_NOTI_TO_PDIC_ATTACH(ATTACHED_DEV_OTG_MUIC);
		muic_manager_switch_path(muic_if, MUIC_PATH_USB_AP);
	} else {
		pr_info("[MUIC] %s USB Killer Detected!!!\n", __func__);
	}

	return 0;
}

static int muic_manager_handle_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct muic_interface_t *muic_if =
		container_of(nb, struct muic_interface_t, manager_nb);
#else
	struct muic_interface_t *muic_if =
		container_of(nb, struct muic_interface_t, pdic_nb);
#endif

#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti =
	    (struct ifconn_notifier_template *)data;
	int attach = IFCONN_NOTIFY_ID_ATTACH;
	int rid = IFCONN_NOTIFY_ID_RID;
	int water = IFCONN_NOTIFY_ID_WATER;
	int otg = IFCONN_NOTIFY_ID_OTG;
	int ta = IFCONN_NOTIFY_ID_TA;
	int role_swap = IFCONN_NOTIFY_ID_ROLE_SWAP;
#else
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;
	int attach = PDIC_NOTIFY_ID_ATTACH;
	int rid = PDIC_NOTIFY_ID_RID;
	int water = PDIC_NOTIFY_ID_WATER;
	int otg = PDIC_NOTIFY_ID_OTG;
	int ta = PDIC_NOTIFY_ID_TA;
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
	int role_swap = PDIC_NOTIFY_ID_ROLE_SWAP;
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (pnoti->dest != PDIC_NOTIFY_DEV_MUIC) {
		pr_info("%s destination id is invalid\n", __func__);
		return 0;
	}
#endif
#endif

	pr_info("%s: Rcvd Noti=> action: %d src:%d dest:%d id:%d sub[%d %d %d], muic_if : %p\n", __func__,
		(int)action, pnoti->src, pnoti->dest, pnoti->id, pnoti->sub1, pnoti->sub2, pnoti->sub3, muic_if);

	muic_manager_show_status(muic_if);

	if (pnoti->id == attach) {
		pr_info("%s: NOTIFY_ID_ATTACH: %s\n", __func__,
			pnoti->sub1 ? "Attached" : "Detached");
		muic_manager_handle_pdic_attach(muic_if, data);
	} else if (pnoti->id == rid) {
		pr_info("%s: NOTIFY_ID_RID\n", __func__);
		muic_manager_handle_pdic_rid(muic_if, data);
	} else if (pnoti->id == water) {

#ifndef CONFIG_SEC_FACTORY
		pr_info("%s: NOTIFY_ID_WATER, boot water : %d\n", __func__, pnoti->sub2);
		if (pnoti->sub2)
			muic_manager_handle_pdic_water_from_boot(muic_if, data);
		else
			muic_manager_handle_pdic_water(muic_if, data);
#else
		pr_info("%s: NOTIFY_ID_WATER\n", __func__);
		muic_manager_handle_pdic_water(muic_if, data);
#endif
	} else if (pnoti->id == otg) {
		pr_info("%s: NOTIFY_ID_OTG\n", __func__);
		muic_manager_handle_otg(muic_if, data);
	} else if (pnoti->id == ta) {
		pr_info("%s: NOTIFY_ID_TA\n", __func__);
#ifndef CONFIG_SEC_FACTORY
#if IS_ENABLED(CONFIG_HV_MUIC_S2MU004_AFC) || IS_ENABLED(CONFIG_MUIC_HV)
		muic_manager_handle_pdic_TA(muic_if, data);
#endif
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
	} else if (pnoti->id == role_swap) {
		pr_info("%s: NOTIFY_ID_ROLE_SWAP\n", __func__);
		muic_manager_handle_pdic_role_swap(muic_if, data);
#endif
	} else {
		pr_info("%s: Undefined Noti. ID\n", __func__);
	}

	muic_manager_show_status(muic_if);

	return NOTIFY_DONE;
}

#ifndef CONFIG_IFCONN_NOTIFIER
void _muic_delayed_notifier(struct work_struct *work)
{
	struct muic_interface_t *muic_if;
	int ret = 0;

	pr_info("%s\n", __func__);

	muic_if = container_of(work, struct muic_interface_t, pdic_work.work);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	ret = manager_notifier_register(&muic_if->manager_nb,
		muic_manager_handle_notification, MANAGER_NOTIFY_PDIC_MUIC);
#else
	ret = pdic_notifier_register(&muic_if->pdic_nb,
		muic_manager_handle_notification, PDIC_NOTIFY_DEV_MUIC);
#endif

	if (ret < 0) {
		pr_info("%s: PDIC Noti. is not ready. Try again in 4sec...\n", __func__);
		schedule_delayed_work(&muic_if->pdic_work, msecs_to_jiffies(4000));
		return;
	}

	pr_info("%s: done.\n", __func__);
}

void muic_manager_register_notifier(struct muic_interface_t *muic_if)
{
	int ret = 0;

	pr_info("%s: Registering PDIC_NOTIFY_DEV_MUIC.\n", __func__);

	muic_manager_init_dev_desc(muic_if);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	ret = manager_notifier_register(&muic_if->manager_nb,
		muic_manager_handle_notification, MANAGER_NOTIFY_PDIC_MUIC);
#else
	ret = pdic_notifier_register(&muic_if->pdic_nb,
		muic_manager_handle_notification, PDIC_NOTIFY_DEV_MUIC);
#endif

	if (ret < 0) {
		pr_info("%s: PDIC Noti. is not ready. Try again in 8sec...\n", __func__);
		INIT_DELAYED_WORK(&muic_if->pdic_work, _muic_delayed_notifier);
		schedule_delayed_work(&muic_if->pdic_work, msecs_to_jiffies(8000));
		return;
	}

	pr_info("%s: done.\n", __func__);
}
#endif
#endif

struct muic_interface_t *muic_manager_init(void *pdata, void *drv_data)
{
#ifdef CONFIG_IFCONN_NOTIFIER
	int ret;
#endif
	struct muic_interface_t *muic_if;
	struct pdic_desc_t *pdic;

	pr_info("%s\n", __func__);

	muic_if = kzalloc(sizeof(*muic_if), GFP_KERNEL);
	if (unlikely(!muic_if)) {
		pr_err("%s failed to allocate driver data\n", __func__);
		return NULL;
	}

	pdic = kzalloc(sizeof(*pdic), GFP_KERNEL);
	if (unlikely(!pdic)) {
		pr_err("%s failed to allocate driver data\n", __func__);
		goto err_pdic_alloc;
	}

	muic_if->pdic = pdic;
	muic_if->muic_data = drv_data;
	muic_if->pdata = pdata;
	muic_if->pdic->rid_desc = pdic_rid_tbl;
	muic_if->opmode = get_pdic_info() & 0xF;
	muic_if->is_pdic_attached = false;
	muic_if->is_pdic_probe = false;
#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
	muic_if->is_afc_reset = false;
	muic_if->is_dcp_charger = false;
	muic_if->is_dcdtmr_intr = false;
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
	muic_if->prswap_status = MUIC_PRSWAP_UNDIFINED;
#endif
#if IS_ENABLED(CONFIG_MUIC_HV)
	muic_if->is_afc_pdic_ready = false;
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#ifdef CONFIG_IFCONN_NOTIFIER
	ret = ifconn_notifier_register(&muic_if->ifconn_nb,
				       muic_manager_handle_notification,
				       IFCONN_NOTIFY_MUIC, IFCONN_NOTIFY_PDIC);
	if (ret) {
		pr_err("%s failed register ifconn notifier\n", __func__);
		goto err_reg_noti;
	}
#else
	if (muic_if->opmode & OPMODE_PDIC)
		muic_manager_register_notifier(muic_if);
	else
		pr_info("OPMODE_MUIC PDIC NOTIFIER is not used\n");
#endif
#endif
	return muic_if;
#ifdef CONFIG_IFCONN_NOTIFIER
err_reg_noti:
	kfree(pdic);
#endif
err_pdic_alloc:
	kfree(muic_if);
	return NULL;
}
EXPORT_SYMBOL(muic_manager_init);

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
	struct muic_interface_t *muic_if = power_supply_get_drvdata(psy);
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_LSI_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
		case POWER_SUPPLY_LSI_PROP_PM_VCHGIN:
			val->intval = muic_if->get_vbus(muic_if->muic_data);
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
	struct muic_interface_t *muic_if = power_supply_get_drvdata(psy);
	int ret;
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_LSI_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
		case POWER_SUPPLY_LSI_PROP_WATER_STATUS:
			if (muic_if->set_water_state)
				muic_if->set_water_state(muic_if->muic_data, val->intval);
			break;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
		case POWER_SUPPLY_LSI_PROP_HICCUP_MODE:
			if (muic_if->set_hiccup_mode)
				muic_if->set_hiccup_mode(muic_if->muic_data, val->intval);
			break;
#endif
		case POWER_SUPPLY_LSI_PROP_PM_VCHGIN:
			MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->pm_chgin_irq,
				muic_if->muic_data, val->intval, &ret);
			break;
		case POWER_SUPPLY_LSI_PROP_PM_FACTORY:
			muic_if->is_bypass = true;
			if (muic_if->set_bypass)
				muic_if->set_bypass(muic_if->muic_data);
			break;
		case POWER_SUPPLY_LSI_PROP_PD_SUPPORT:
			muic_if->is_pdic_probe = true;
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

int muic_manager_psy_init(struct muic_interface_t *muic_if, struct device *parent)
{
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	if (muic_if == NULL || parent == NULL) {
		pr_err("%s NULL data\n", __func__);
		return -1;
	}

	muic_if->psy_muic_desc.name           = "muic-manager";
	muic_if->psy_muic_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	muic_if->psy_muic_desc.get_property   = muic_manager_get_property;
	muic_if->psy_muic_desc.set_property   = muic_manager_set_property;
	muic_if->psy_muic_desc.properties     = muic_props;
	muic_if->psy_muic_desc.num_properties = ARRAY_SIZE(muic_props);

	psy_cfg.drv_data = muic_if;
	psy_cfg.supplied_to = muic_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(muic_supplied_to);

	muic_if->psy_muic = power_supply_register(parent, &muic_if->psy_muic_desc, &psy_cfg);
	if (IS_ERR(muic_if->psy_muic)) {
		ret = (int)PTR_ERR(muic_if->psy_muic);
		pr_err("%s: Failed to Register psy_muic, ret : %d\n", __func__, ret);
	}
	return ret;
}
EXPORT_SYMBOL(muic_manager_psy_init);
#endif

void muic_manager_exit(struct muic_interface_t *muic_if)
{
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
	power_supply_unregister(muic_if->psy_muic);
#endif
	kfree(muic_if);
}
EXPORT_SYMBOL(muic_manager_exit);

static int __init muic_manager_module_init(void)
{
	pr_info("%s: init\n", __func__);

	return 0;
}

static void __exit muic_manager_module_exit(void)
{
	pr_info("%s: exit\n", __func__);
}

device_initcall(muic_manager_module_init);
module_exit(muic_manager_module_exit);

MODULE_AUTHOR("Samsung USB Team");
MODULE_DESCRIPTION("Muic Manager");
MODULE_LICENSE("GPL");
