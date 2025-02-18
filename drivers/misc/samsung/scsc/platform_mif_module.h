/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __PLATFORM_MIF_MODULE_H
#define __PLATFORM_MIF_MODULE_H

#define DRV_NAME "scsc_wlbt"
#define IRQ_NAME_MBOX "wlan_mbox_irq"
#define IRQ_NAME_MBOX_WPAN "bt_mbox_irq"
#define IRQ_NAME_WDOG "wlbt_wdog_irq"
#if defined(CONFIG_WLBT_DCXO_TUNE)
#define IRQ_NAME_MBOX_APM "apm_mbox_irq"
#endif

#endif /* __PLATFORM_MIF_MODULE_H */
