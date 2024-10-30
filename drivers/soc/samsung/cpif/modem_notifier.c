// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 Samsung Electronics.
 *
 */

#include <stdarg.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/notifier.h>

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX)
#include <sound/samsung/abox.h>
#endif
#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_notifier.h"

void modem_notify_event(enum modem_event evt, struct modem_ctl *mc)
{
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX)
	static enum abox_modem_event abox_evt[] = {
		[MODEM_EVENT_RESET] = ABOX_MODEM_EVENT_RESET,
		[MODEM_EVENT_EXIT] = ABOX_MODEM_EVENT_EXIT,
		[MODEM_EVENT_ONLINE] = ABOX_MODEM_EVENT_ONLINE,
		[MODEM_EVENT_OFFLINE] = ABOX_MODEM_EVENT_OFFLINE,
		[MODEM_EVENT_WATCHDOG] = ABOX_MODEM_EVENT_WATCHDOG,
	};
#endif

#if IS_ENABLED(CONFIG_REINIT_VSS)
	switch (evt) {
	case MODEM_EVENT_RESET:
	case MODEM_EVENT_EXIT:
	case MODEM_EVENT_WATCHDOG:
		reinit_completion(&mc->vss_stop);
		break;
	default:
		break;
	}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX)
	mif_info("event notify cp_evt:%d abox_evt:%d\n", evt, abox_evt[evt]);
	abox_notify_modem_event(abox_evt[evt]);
#endif
}
EXPORT_SYMBOL(modem_notify_event);

#if IS_ENABLED(CONFIG_SUSPEND_DURING_VOICE_CALL)
void modem_voice_call_notify_event(enum modem_voice_call_event evt)
{
	/* ToDo */
}
EXPORT_SYMBOL(modem_voice_call_notify_event);
#endif
