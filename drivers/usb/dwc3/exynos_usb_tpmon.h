#ifndef __EXYNOS_USB_TPMON_H__
#define __EXYNOS_USB_TPMON_H__

extern struct dwc3_exynos *g_dwc3_exynos;

//void usb_tpmon_check_tp(void *data, int data_size);
void usb_tpmon_init_data(void);
void usb_tpmon_init(void);
void usb_tpmon_exit(void);
void usb_tpmon_open(void);
void usb_tpmon_close(void);

#endif /* __EXYNOS_USB_TPMON_H__ */
