// SPDX-License-Identifier: GPL-2.0
/**
 * dwc3-exynos.h - Samsung EXYNOS DWC3 Specific Glue layer header
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Anton Tikhomirov <av.tikhomirov@samsung.com>
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

#ifndef __LINUX_USB_DWC3_EXYNOS_H
#define __LINUX_USB_DWC3_EXYNOS_H

/* Exynos Specific Register Definition */

/* LINK Registers */
#define DWC3_LU3LFPSRXTIM	0xd010
#define DWC3_LSKIPFREQ		0xd020
#define DWC3_LLUCTL		0xd024
#define DWC3_BU31RHBDBG		0xd800

/* Link Register - LLUCTL */
#define DWC3_PENDING_HP_TIMER_US(n)	((n) << 16)
#define DWC3_LLUCTL_LTSSM_TIMER_OVRRD	BIT(23)
#define DWC3_EN_US_HP_TIMER		BIT(15)
#define DWC3_LLUCTL_PIPE_RESET		BIT(7)
#define DWC3_FORCE_GEN1			BIT(10)
#define DWC3_LLUCTL_TX_TS1_CNT(n)	((n) << 0)
#define DWC3_LLUCTL_TX_TS1_CNT_MASK	DWC3_LLUCTL_TX_TS1_CNT(0x1f)

/* Link Register - LSKIPFREQ */
#define DWC3_PM_ENTRY_TIMER_US(n)	((n) << 20)
#define DWC3_PM_ENTRY_TIMER_US_MASK	DWC3_PM_ENTRY_TIMER_US(0xf)
#define DWC3_PM_LC_TIMER_US(n)		((n) << 24)
#define DWC3_PM_LC_TIMER_US_MASK	DWC3_PM_LC_TIMER_US(0x7)
#define DWC3_EN_PM_TIMER_US		BIT(27)

/* Global User Control 1 Register */
#define DWC3_GUCTL1_PARKMODE_DISABLE_SS	BIT(17)
#define DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS	BIT(28)
#define DWC3_GUCTL1_DEV_L1_EXIT_BY_HW	BIT(24)
#define DWC3_GUCTL1_DEV_DECOUPLE_L1L2_EVT	BIT(31)
#define DWC3_GUCTL1_IP_GAP_ADD_ON(n)		((n) << 21)
#define DWC3_GUCTL1_IP_GAP_ADD_ON_MASK	(DWC3_GUCTL1_IP_GAP_ADD_ON(0x7))

/* Debug Register */
#define DWC3_BU31RHBDBG_TOUTCTL	(0x1 << 3)

#define DWC3_EXYNOS_IGNORE_CORE_OPS	0xff

#define USB_BUS_CLOCK_DELAY_MS 3000

/* USB Suspend State related to System Sleep */
#define USB_NORMAL 0
#define USB_SUSPEND_PREPARE 1
#define USB_POST_SUSPEND 2

struct dwc3_exynos_rsw {
	struct otg_fsm		*fsm;
	struct work_struct	work;
};

struct dwc3_exynos_config { /* Exynos Specific Configuations */
	bool adj_sof_accuracy;
	bool sparse_transfer_control;
	bool no_extra_delay;
	bool ux_exit_in_px_quirk;
	bool elastic_buf_mode_quirk;
	bool force_gen1;
	bool u1u2_exitfail_to_recov_quirk;
	u32 usb_host_device_timeout;
	u32 suspend_clk_freq;
	bool usb20_pkt_retry_disable;
};

struct dwc3_exynos {
	struct platform_device	*usb2_phy;
	struct platform_device	*usb3_phy;
	struct device		*dev;
	struct dwc3		*dwc;

	struct clk		**clocks;
	struct clk		*bus_clock;
	struct clk		*sclk_clock;

	struct regulator	*vdd33;
	struct regulator	*vdd10;

	int			idle_ip_index;
	unsigned long		bus_clock_rate;

	struct dwc3_exynos_rsw	rsw;
	struct dwc3_otg		*dotg;

	/* To check USB connection */
	int			vbus_state;
	struct dwc3_exynos_config config;
	int			usb_host_ready;

	struct delayed_work	usb_qos_lock_delayed_work;
	struct workqueue_struct	*int_qos_lock_wq;
	struct work_struct	int_qos_work;
	struct work_struct		int_kprobe_work;
	int			is_perf;
	int			lazy_vbus_up;
};

struct usb_xhci_pre_alloc {
	u8 *pre_dma_alloc;
	u64 offset;

	dma_addr_t	dma;
};
bool dwc3_exynos_rsw_available(struct device *dev);
int dwc3_exynos_rsw_setup(struct device *dev, struct otg_fsm *fsm);
void dwc3_exynos_rsw_exit(struct device *dev);
int dwc3_exynos_rsw_start(struct device *dev);
void dwc3_exynos_rsw_stop(struct device *dev);
int dwc3_exynos_id_event(struct device *dev, int state);
int dwc3_exynos_vbus_event(struct device *dev, bool vbus_active);
int dwc3_exynos_start_ldo(struct device *dev, bool on);
int dwc3_exynos_phy_enable(int owner, bool on);
int dwc3_otg_is_usb_ready(void);
int dwc3_exynos_get_idle_ip_index(struct device *dev);
int dwc3_exynos_set_bus_clock(struct device *dev, int clk_level);
int dwc3_exynos_set_sclk_clock(struct device *dev);
unsigned int of_usb_get_suspend_clk_freq(struct device *dev);
int dwc3_probe(struct platform_device *pdev,
			struct dwc3_exynos *exynos);
void dwc3_core_exit_mode(struct dwc3 *dwc);
void dwc3_free_event_buffers(struct dwc3 *dwc);
void dwc3_free_scratch_buffers(struct dwc3 *dwc);

int dwc3_gadget_vbus_session(struct dwc3_exynos *exynos, int is_active);
int dwc3_exynos_core_init(struct dwc3 *dwc, struct dwc3_exynos *exynos);
void dwc3_exynos_gadget_disconnect_proc(struct dwc3 *dwc);
int dwc3_core_susphy_set(struct dwc3 *dwc, int on);
extern void exynos_usbdrd_shutdown_notice(int shutdown);
extern void exynos_usbdrd_dp_use_notice(int lane);
#if defined(CONFIG_OTG_CDP_SUPPORT)
void exynos_usbdrd_cdp_set(struct phy *phy, int val);
#endif
#endif /* __LINUX_USB_DWC3_EXYNOS_H */

