#include <linux/usb/typec/slsi_common/usbpd_ext_common.h>

#ifndef __LEGACY_USBPD_EXT_H__
#define __LEGACY_USBPD_EXT_H__

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
extern struct usbpd_data *legacy_g_pd_data;
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
extern void legacy_pdic_event_work(void *data, int dest, int id, int attach, int event, int sub);
extern void legacy_pdo_ctrl_by_flash(bool mode);
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
extern int legacy_typec_init(struct usbpd_data *_data);
#endif
#endif
