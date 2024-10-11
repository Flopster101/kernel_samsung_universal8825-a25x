// SPDX-License-Identifier: GPL-2.0

/*
 * xHCI host controller exynos driver
 *
 *
 */

#include "xhci.h"
#include "../dwc3/dwc3-exynos.h"

/*
 * Sometimes deadlock occurred between hub_event and remove_hcd.
 * In order to prevent it, waiting for completion of hub_event was added.
 * This is a timeout (300msec) value for the waiting.
 */
#define XHCI_HUB_EVENT_TIMEOUT	(300)

#define XHCI_USE_URAM_FOR_EXYNOS_AUDIO	BIT_ULL(62)
#define XHCI_L2_SUPPORT			BIT_ULL(63)

#define PORTSC_OFFSET		0x430

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO

/* EXYNOS uram memory map */
#if defined(CONFIG_SOC_S5E9925)
#define EXYNOS_URAM_ABOX_EVT_RING_ADDR	0x02a00000
#define EXYNOS_URAM_ISOC_OUT_RING_ADDR	0x02a01000
#define EXYNOS_URAM_ISOC_IN_RING_ADDR	0x02a02000
#define EXYNOS_URAM_DEVICE_CTX_ADDR	0x02a03000
#define EXYNOS_URAM_DCBAA_ADDR		0x02a03880
#define EXYNOS_URAM_ABOX_ERST_SEG_ADDR	0x02a03C80
#elif defined(CONFIG_SOC_S5E8825)
#define EXYNOS_URAM_ABOX_EVT_RING_ADDR	0x13300000
#define EXYNOS_URAM_ISOC_OUT_RING_ADDR	0x13301000
#define EXYNOS_URAM_ISOC_IN_RING_ADDR	0x13302000
#define EXYNOS_URAM_DEVICE_CTX_ADDR	0x13303000
#define EXYNOS_URAM_DCBAA_ADDR		0x13303880
#define EXYNOS_URAM_ABOX_ERST_SEG_ADDR	0x13303C80
#else
#error
#endif
#endif

enum usb_port_state {
	PORT_EMPTY = 0,		/* OTG only */
	PORT_USB2,		/* usb 2.0 device only */
	PORT_USB3,		/* usb 3.0 device only */
	PORT_HUB,		/* usb hub single */
	PORT_DP			/* DP device */
};

struct xhci_hcd_exynos {
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	struct	xhci_intr_reg __iomem *ir_set_audio;
#endif

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	struct xhci_ring	*event_ring_audio;
	struct xhci_erst	erst_audio;
	dma_addr_t save_dma;
	void *save_addr;
	dma_addr_t out_dma;
	void *out_addr;
	dma_addr_t in_dma;
	void *in_addr;
#endif
	struct device		*dev;
	struct usb_hcd		*hcd;
	struct usb_hcd		*shared_hcd;

	struct wakeup_source *main_wakelock; /* Wakelock for HS HCD */
	struct wakeup_source *shared_wakelock; /* Wakelock for SS HCD */

	void __iomem		*usb3_portsc;
	spinlock_t		xhcioff_lock;
	int			port_off_done;
	int			port_set_delayed;
	u32			portsc_control_priority;
	enum usb_port_state	port_state;
	int			usb3_phy_control;

	struct usb_xhci_pre_alloc	*xhci_alloc;
	struct phy		*phy_usb2;
	struct phy		*phy_usb3;

	/* This flag is used to check first allocation for URAM */
	bool			exynos_uram_ctx_alloc;
	bool			exynos_uram_isoc_out_alloc;
	bool			exynos_uram_isoc_in_alloc;
	u8			*usb_audio_ctx_addr;
	u8			*usb_audio_isoc_out_addr;
	u8			*usb_audio_isoc_in_addr;

};

struct usb_phy_roothub {
	struct phy		*phy;
	struct list_head	list;
};

struct xhci_exynos_priv {
	const char *firmware_name;
	unsigned long long quirks;
	struct xhci_vendor_data *vendor_data;
	int (*plat_setup)(struct usb_hcd *);
	void (*plat_start)(struct usb_hcd *);
	int (*init_quirk)(struct usb_hcd *);
	int (*suspend_quirk)(struct usb_hcd *);
	int (*resume_quirk)(struct usb_hcd *);
	struct xhci_hcd_exynos *xhci_exynos;
};

#define hcd_to_xhci_exynos_priv(h) ((struct xhci_exynos_priv *)hcd_to_xhci(h)->priv)
#define xhci_to_exynos_priv(x) ((struct xhci_exynos_priv *)(x)->priv)

extern int is_otg_only;
extern int exynos_usbdrd_phy_vendor_set(struct phy *phy, int is_enable,
					int is_cancel);
extern int get_idle_ip_index(void);

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
int xhci_store_hw_info(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_set_deq(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
		unsigned int last_ep, struct usb_device *udev);
#endif

int xhci_hub_check_speed(struct usb_hcd *hcd);
int xhci_check_usbl2_support(struct usb_hcd *hcd);
int xhci_wake_lock(struct usb_hcd *hcd, int is_lock);
void xhci_usb_parse_endpoint(struct usb_device *udev, struct usb_endpoint_descriptor *desc, int size);

extern int exynos_usbdrd_phy_vendor_set(struct phy *phy, int is_enable,
		int is_cancel);
extern int exynos_usbdrd_phy_tune(struct phy *phy, int phy_state);
void usb_power_notify_control(int owner, int on);
