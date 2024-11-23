/*
 * sm5714-muic_pdic.c
 *
 * Copyright (C) 2018 Samsung Electronics
 * Heegon Lee <heegon.lee@samsung.com>
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>
#include <linux/string.h>

#include <linux/muic/common/muic.h>
#if defined(CONFIG_SEC_FACTORY)
#include <linux/mfd/sm/sm5714/sm5714-private.h>
#endif
#include <linux/muic/sm/sm5714/sm5714-muic.h>
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

#if !defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/sec_pd.h>
#endif
#if defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#endif

static void sm5714_muic_init_pdic_info_data(struct sm5714_muic_data *muic_data)
{
	pr_info("%s\n", __func__);
	muic_data->pdic_info_data.pdic_evt_rid = RID_OPEN;
	muic_data->pdic_info_data.pdic_evt_rprd = 0;
	muic_data->pdic_info_data.pdic_evt_roleswap = 0;
	muic_data->pdic_info_data.pdic_evt_dcdcnt = 0;
	muic_data->pdic_info_data.pdic_evt_attached = MUIC_PDIC_NOTI_UNDEFINED;
	muic_data->pdic_afc_state = SM5714_MUIC_AFC_NORMAL;
}

static void sm5714_muic_handle_pdic_detach(struct sm5714_muic_data *muic_data)
{
	pr_info("%s\n", __func__);
	muic_data->pdic_info_data.pdic_evt_rprd = 0;
	muic_data->pdic_info_data.pdic_evt_roleswap = 0;
	muic_data->pdic_info_data.pdic_evt_dcdcnt = 0;
	muic_data->pdic_info_data.pdic_evt_attached = MUIC_PDIC_NOTI_DETACH;
	muic_data->pdic_afc_state = SM5714_MUIC_AFC_NORMAL;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static void sm5714_muic_handle_pdic_ABNORMAL(struct sm5714_muic_data *muic_data)
{
	muic_data->pdic_afc_state = SM5714_MUIC_AFC_ABNORMAL;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		muic_afc_set_voltage(5);
		break;
	default:
		break;
	}
}

static void sm5714_muic_handle_RPLEVEL(struct sm5714_muic_data *muic_data)
{
	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		break;
	default:
		muic_data->pdic_afc_state = SM5714_MUIC_AFC_ABNORMAL;
		break;
	}
}
#endif

static int sm5714_muic_handle_pdic_ATTACH(struct sm5714_muic_data *muic_data,
		PD_NOTI_ATTACH_TYPEDEF *pnoti)
{
	bool need_to_run_work = false;

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n",
			__func__, pnoti->src, pnoti->dest, pnoti->id,
			pnoti->attach, pnoti->cable_type, pnoti->rprd);

	/* Attached */
	if (pnoti->attach) {
		pr_info("%s: Attach, cable type=%d\n", __func__,
				pnoti->cable_type);

		muic_data->pdic_info_data.pdic_evt_attached =
			MUIC_PDIC_NOTI_ATTACH;

		if (muic_data->pdic_info_data.pdic_evt_roleswap) {
			pr_info("%s: roleswap event, attach USB\n", __func__);
			muic_data->pdic_info_data.pdic_evt_roleswap = 0;
			need_to_run_work = true;
		}

		if (pnoti->rprd) {
			pr_info("%s: RPRD\n", __func__);
			muic_data->pdic_info_data.pdic_evt_rprd = 1;
			need_to_run_work = true;
		}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (pnoti->cable_type == RP_CURRENT_ABNORMAL)
			sm5714_muic_handle_pdic_ABNORMAL(muic_data);
		else if (pnoti->cable_type == RP_CURRENT_LEVEL2 ||
				pnoti->cable_type == RP_CURRENT_LEVEL3)
			sm5714_muic_handle_RPLEVEL(muic_data);
#endif

		if (pnoti->cable_type == ATTACHED_DEV_TIMEOUT_OPEN_MUIC) {
			muic_data->pdic_info_data.pdic_evt_dcdcnt = 1;
			need_to_run_work = true;
		}

		muic_data->is_water_detect = false;
	} else {
		if (pnoti->rprd) {
			/* Role swap detach: attached=0, rprd=1 */
			pr_info("%s: role swap event\n", __func__);
			muic_data->pdic_info_data.pdic_evt_roleswap = 1;
		} else {
			/* Detached */
			if (muic_data->pdic_info_data.pdic_evt_rprd)
				need_to_run_work = true;

			if (muic_data->need_to_path_open)
				need_to_run_work = true;

			if (muic_data->pdic_info_data.pdic_evt_dcdcnt)
				need_to_run_work = true;

			sm5714_muic_handle_pdic_detach(muic_data);
		}
	}

	/* run muic event handler */
	if (need_to_run_work) {
		pr_info("%s: do workqueue\n", __func__);
		schedule_work(&(muic_data->muic_event_work));
	}

	if (!muic_data->is_pdic_ready) {
		muic_data->is_pdic_ready = true;

		if (muic_data->is_charger_ready) {
			if ((muic_data->attached_dev == ATTACHED_DEV_TA_MUIC) ||
					(muic_data->attached_dev == ATTACHED_DEV_UNOFFICIAL_TA_MUIC))
				schedule_work(&muic_data->muic_afc_init_work);
		}
	}

	return 0;
}

static int sm5714_muic_handle_pdic_RID(struct sm5714_muic_data *muic_data,
		PD_NOTI_RID_TYPEDEF *pnoti)
{
	int prev_rid = muic_data->pdic_info_data.pdic_evt_rid;
	int rid = pnoti->rid;
#if defined(CONFIG_SEC_FACTORY)
	int intr2 = 0;
#endif

	pr_info("%s: src:%d dest:%d id:%d rid:%d sub2:%d sub3:%d\n",
			__func__, pnoti->src, pnoti->dest, pnoti->id,
			pnoti->rid, pnoti->sub2, pnoti->sub3);

	if (rid > RID_OPEN || rid <= RID_UNDEFINED) {
		pr_info("%s: Out of range of RID(%d)\n", __func__, rid);
		return 0;
	}

	muic_data->pdic_info_data.pdic_evt_rid = rid;

	switch (rid) {
	case RID_000K:
	case RID_255K:
	case RID_301K:
	case RID_523K:
	case RID_619K:
#if defined(CONFIG_SEC_FACTORY)
		if (!muic_data->afc_irq_disabled) {
			disable_irq(muic_data->irqs.irq_afc_ta_attached);
			pr_info("%s: disable_irq (%d)\n", __func__,
					muic_data->irqs.irq_afc_ta_attached);
			muic_data->afc_irq_disabled = true;
		}
#endif
		break;
	case RID_OPEN:
#if defined(CONFIG_SEC_FACTORY)
		if (muic_data->afc_irq_disabled) {
			intr2 = sm5714_i2c_read_byte(muic_data->i2c,
					SM5714_MUIC_REG_INT2);
			pr_info("%s: REG_INT2 (0x%x)\n", __func__, intr2);

			enable_irq(muic_data->irqs.irq_afc_ta_attached);
			pr_info("%s: enable_irq (%d)\n", __func__,
					muic_data->irqs.irq_afc_ta_attached);
			muic_data->afc_irq_disabled = false;
		}
#endif
		break;
	default:
		pr_err("%s:Not determined now\n", __func__);
		break;
	}

	if ((prev_rid != rid) || (rid == RID_619K)) {
		pr_info("%s: do workqueue\n", __func__);
		schedule_work(&(muic_data->muic_event_work));
	}

	return 0;
}

static int sm5714_muic_handle_pdic_WATER(struct sm5714_muic_data *muic_data,
		PD_NOTI_ATTACH_TYPEDEF *pnoti)
{
	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n",
			__func__, pnoti->src, pnoti->dest, pnoti->id,
			pnoti->attach, pnoti->cable_type, pnoti->rprd);

	if (pnoti->attach == PDIC_NOTIFY_ATTACH) {
		muic_data->is_water_detect = true;
		muic_set_hiccup_mode(0);
		pr_info("%s: Water detect\n", __func__);
	} else {
		muic_data->is_water_detect = false;
		pr_info("%s: Dry detect\n", __func__);
	}

	return 0;
}

static int sm5714_muic_handle_pdic_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct sm5714_muic_data *muic_data = container_of(nb,
			struct sm5714_muic_data, manager_nb);
#else
	struct sm5714_muic_data *muic_data = container_of(nb,
			struct sm5714_muic_data, pdic_nb);
#endif

	pr_info("%s: action:%d src:%d dest:%d id:%d sub[%d %d %d]\n", __func__,
		(int)action, pnoti->src, pnoti->dest, pnoti->id,
		pnoti->sub1, pnoti->sub2, pnoti->sub3);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (pnoti->dest != PDIC_NOTIFY_DEV_MUIC) {
		pr_info("%s destination id is invalid\n", __func__);
		return 0;
	}
#endif
	muic_data->pdic_evt_id = pnoti->id;

	switch (pnoti->id) {
	case PDIC_NOTIFY_ID_ATTACH:
		pr_info("%s: PDIC_NOTIFY_ID_ATTACH: %s\n", __func__,
				pnoti->sub1 ? "Attached" : "Detached");
		sm5714_muic_handle_pdic_ATTACH(muic_data,
				(PD_NOTI_ATTACH_TYPEDEF *)pnoti);
		break;
	case PDIC_NOTIFY_ID_RID:
		pr_info("%s: PDIC_NOTIFY_ID_RID\n", __func__);
		sm5714_muic_handle_pdic_RID(muic_data,
				(PD_NOTI_RID_TYPEDEF *)pnoti);
		break;
	case PDIC_NOTIFY_ID_WATER:
		pr_info("%s: PDIC_NOTIFY_ID_WATER\n", __func__);
		sm5714_muic_handle_pdic_WATER(muic_data,
				(PD_NOTI_ATTACH_TYPEDEF *)pnoti);
		break;
	default:
		pr_info("%s: Undefined Noti. ID\n", __func__);
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

void sm5714_muic_register_pdic_notifier(struct sm5714_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s: Registering PDIC_NOTIFY_DEV_MUIC.\n", __func__);

	sm5714_muic_init_pdic_info_data(muic_data);
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	ret = manager_notifier_register(&muic_data->manager_nb,
		sm5714_muic_handle_pdic_notification, MANAGER_NOTIFY_PDIC_MUIC);
#else
	ret = pdic_notifier_register(&muic_data->pdic_nb,
		sm5714_muic_handle_pdic_notification, PDIC_NOTIFY_DEV_MUIC);
#endif
	if (ret < 0) {
		pr_info("%s: PDIC Noti. is not ready\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}

void sm5714_muic_unregister_pdic_notifier(struct sm5714_muic_data *muic_data)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	ret = manager_notifier_unregister(&muic_data->manager_nb);
#else
	ret = pdic_notifier_unregister(&muic_data->pdic_nb);
#endif
	if (ret < 0) {
		pr_info("%s: PDIC Noti. is not ready\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}
