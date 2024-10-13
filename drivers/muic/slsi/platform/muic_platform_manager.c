// SPDX-License-Identifier: GPL-2.0
/*
 * muic_platform_manager.c
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/string.h>
#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#include <linux/muic/slsi/platform/muic_platform_manager.h>
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
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

static void muic_manager_handle_pdic_detach(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;

	pr_info("%s\n", __func__);

	/* Reset status & flags */
	pdic->attached_dev = 0;
	pdic->pdic_evt_rid = 0;
	pdic->pdic_evt_rprd = 0;
	pdic->pdic_evt_roleswap = 0;
	pdic->pdic_evt_attached = MUIC_PDIC_NOTI_UNDEFINED;
	pdic->pdic_evt_dcdcnt = 0;

	muic_if->is_water = false;
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static void muic_manager_show_status(struct muic_interface_t *muic_if)
{
	struct pdic_desc_t *pdic = muic_if->pdic;

	pr_info("%s: attached_dev:%d rid:%d rprd:%d attached:%d\n", __func__,
			pdic->attached_dev, pdic->pdic_evt_rid, pdic->pdic_evt_rprd,
			pdic->pdic_evt_attached);
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
	pdic->pdic_evt_dcdcnt = 0;

	muic_if->is_water = false;
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static int muic_manager_handle_pdic_attach(struct muic_interface_t *muic_if, void *data)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
#endif
	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	pdic->pdic_evt_attached = pnoti->attach ?
		MUIC_PDIC_NOTI_ATTACH : MUIC_PDIC_NOTI_DETACH;

	if (pdic->pdic_evt_attached == MUIC_PDIC_NOTI_ATTACH) {
		pr_info("%s: Attach\n", __func__);

		if (pdic->pdic_evt_roleswap) {
			pr_info("%s: roleswap event, attach USB\n", __func__);
			pdic->pdic_evt_roleswap = 0;
		}

		if (pnoti->rprd) {
			pr_info("%s: RPRD\n", __func__);
			pdic->pdic_evt_rprd = 1;
			muic_platform_send_pdic_event(muic_if, EVENT_PDIC_RPRD);
		}
		/* PDIC ATTACH means NO WATER */
		muic_if->is_water = false;
		muic_platform_send_pdic_event(muic_if, EVENT_PDIC_ATTACH);
	} else {
		if (pnoti->rprd) {
			/* Role swap detach: attached=0, rprd=1 */
			pr_info("%s: role swap event\n", __func__);
			pdic->pdic_evt_roleswap = 1;
		} else {
			/* Detached */
			muic_manager_handle_pdic_detach(muic_if);
			muic_platform_send_pdic_event(muic_if, EVENT_PDIC_DETACH);
		}
	}
	return 0;
}

static int muic_manager_handle_pdic_rid(struct muic_interface_t *muic_if, void *data)
{
	struct pdic_desc_t *pdic = muic_if->pdic;
	int rid, ret = 0;
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
		ret = -E2BIG;
		goto err;
	}

	if (pdic->pdic_evt_attached != MUIC_PDIC_NOTI_ATTACH) {
		pr_info("%s: RID but No ATTACH->discarded\n", __func__);
		ret = -EPERM;
		goto err;
	}

	switch (rid) {
	case PDIC_RID_000K:
	case PDIC_RID_255K:
	case PDIC_RID_301K:
	case PDIC_RID_523K:
	case PDIC_RID_619K:
	case PDIC_RID_OPEN:
		pdic->pdic_evt_rid = rid;
		break;
	default:
		pr_err("%s:Undefined RID\n", __func__);
		ret = -EPERM;
		goto err;
	}

	muic_platform_send_pdic_event(muic_if, EVENT_PDIC_RID);
err:
	return ret;
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
	pr_info("%s: Water detect : %s\n", __func__, pnoti->attach ? "en":"dis");

	muic_if->is_water = pnoti->attach;
	muic_platform_send_pdic_event(muic_if, EVENT_PDIC_WATER);
	return 0;
}

static int muic_manager_handle_pdic_TA(struct muic_interface_t *muic_if, void *data)
{
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_ATTACH_TYPEDEF *pnoti = (PD_NOTI_ATTACH_TYPEDEF *)data;
#endif

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id, pnoti->attach, pnoti->cable_type, pnoti->rprd);

	muic_platform_send_pdic_event(muic_if, EVENT_PDIC_TA);
	return 0;
}

static int muic_manager_handle_pdic_otg(struct muic_interface_t *muic_if, void *data)
{
	int ret = 0;
#ifdef CONFIG_IFCONN_NOTIFIER
	struct ifconn_notifier_template *pnoti = (struct ifconn_notifier_template *)data;
#else
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;
#endif

	pr_info("%s sub1:%d\n", __func__, pnoti->sub1);
	if (pnoti->sub1 == false)
		return ret;

	ret = muic_platform_send_pdic_event(muic_if, EVENT_PDIC_OTG);
	return ret;
}

static int muic_manager_handle_pdic_role_swap(struct muic_interface_t *muic_if, void *data)
{
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;
	struct pdic_desc_t *pdic = muic_if->pdic;

	if (pdic->pdic_evt_attached != MUIC_PDIC_NOTI_ATTACH)
		return 0;

	pr_info("%s: src:%d dest:%d sub1:%d\n", __func__, pnoti->src, pnoti->dest,
		pnoti->sub1);

	muic_platform_send_pdic_event(muic_if, EVENT_PDIC_ROLE_SWAP);
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
	int ta = IFCONN_NOTIFY_ID_TA;
#else
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;
	int attach = PDIC_NOTIFY_ID_ATTACH;
	int rid = PDIC_NOTIFY_ID_RID;
	int water = PDIC_NOTIFY_ID_WATER;
	int ta = PDIC_NOTIFY_ID_TA;
	int otg = PDIC_NOTIFY_ID_OTG;
	int role_swap = PDIC_NOTIFY_ID_ROLE_SWAP;
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (pnoti->dest != PDIC_NOTIFY_DEV_MUIC) {
		pr_info("%s destination id is invalid\n", __func__);
		return 0;
	}
#endif
#endif

	pr_info("%s: Rcvd Noti=> action: %d src:%d dest:%d id:%d sub[%d %d %d], muic_if : %pK\n", __func__,
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
		pr_info("%s: NOTIFY_ID_WATER\n", __func__);
		muic_manager_handle_pdic_water(muic_if, data);
	} else if (pnoti->id == ta) {
		pr_info("%s: NOTIFY_ID_TA\n", __func__);
		muic_manager_handle_pdic_TA(muic_if, data);
	} else if (pnoti->id == otg) {
		pr_info("%s: NOTIFY_ID_OTG\n", __func__);
		muic_manager_handle_pdic_otg(muic_if, data);
	} else if (pnoti->id == role_swap) {
		pr_info("%s: NOTIFY_ID_ROLE_SWAP\n", __func__);
		muic_manager_handle_pdic_role_swap(muic_if, data);
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
void muic_manager_unregister_notifier(struct muic_interface_t *muic_if)
{
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_unregister(&muic_if->manager_nb);
#else
	pdic_notifier_unregister(&muic_if->pdic_nb);
#endif
}
#endif
#endif

struct muic_interface_t *register_muic_platform_manager
		(struct muic_share_data *sdata)
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
	muic_if->sdata = sdata;

	muic_if->pdic->rid_desc = pdic_rid_tbl;
	muic_if->opmode = get_pdic_info() & 0xF;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
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
EXPORT_SYMBOL_GPL(register_muic_platform_manager);

void unregister_muic_platform_manager(struct muic_share_data *sdata)
{
	struct muic_interface_t *muic_if = sdata->muic_if;

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	ifconn_notifier_unregister(&muic_if->ifconn_nb);
#else
	muic_manager_unregister_notifier(muic_if);
#endif
#endif

	kfree(muic_if->pdic);
	kfree(muic_if);
}
EXPORT_SYMBOL_GPL(unregister_muic_platform_manager);

