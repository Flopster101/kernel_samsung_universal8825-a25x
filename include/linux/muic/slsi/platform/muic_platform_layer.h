/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/linux/muic/lsi/muic_platform_layer.h
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
#ifndef __MUIC_PLATFORM_LAYER_H__
#define __MUIC_PLATFORM_LAYER_H__

#include <linux/muic/common/muic.h>
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
#include <linux/usb/typec/manager/if_cb_manager.h>
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
#include <linux/power_supply.h>
#endif

enum muic_platform_vbus {
	MPL_VBUS_LOW = 0,
	MPL_VBUS_HIGH,
};

enum pdic_events {
	EVENT_PDIC_RPRD,
	EVENT_PDIC_ATTACH,
	EVENT_PDIC_DETACH,
	EVENT_PDIC_RID,
	EVENT_PDIC_WATER,
	EVENT_PDIC_TA,
	EVENT_PDIC_OTG,
	EVENT_PDIC_ROLE_SWAP,
	EVENT_PDIC_MAX,
};

struct muic_ic_ops {
	int (*get_vbus_state)(void *mdata);
	int (*set_gpio_uart_sel)(void *mdata, int uart_path);
	int (*set_gpio_usb_sel)(void *mdata, int usb_path);
	int (*set_switch_to_open)(void *mdata);
	int (*set_switch_to_uart)(void *mdata, int uart_path);
	int (*set_switch_to_usb)(void *mdata, int usb_path);
	int (*check_usb_killer)(void *mdata);
	void (*set_bypass)(void *mdata);

	void (*set_chg_det)(void *mdata, bool en);
	void (*prswap_work)(void *mdata, int mode);

	int (*set_water_detect)(void *mdata, bool val);
	int (*set_hiccup_mode)(void *mdata, bool en);
	int (*get_hiccup_mode)(void *mdata);
	int (*set_hiccup)(void *mdata, bool en);
	void (*set_water_state)(void *, bool en);

	int (*show_register)(void *mdata, char *mesg);
	int (*get_adc)(void *mdata);

	int (*set_jig_ctrl_on)(void *mdata);
	int (*set_afc_ready)(void *mdata);
	int (*handle_hv_work)(void *mdata);
	int (*afc_get_voltage)(void *mdata);
	int (*afc_set_voltage)(void *mdata, int vol);
	void (*change_afc_voltage)(void *mdata, int tx_data);
	muic_attached_dev_t (*check_id_err)(void *mdata, muic_attached_dev_t new_dev);
	int (*reset_hvcontrol_reg)(void *mdata);
	void (*set_chgtype_usrcmd)(void *mdata);
	void (*hv_reset)(void *mdata);
	void (*hv_dcp_charger)(void *mdata);
	void (*hv_fast_charge_adaptor)(void *mdata);
	void (*hv_fast_charge_communication)(void *mdata);
	void (*hv_afc_5v_charger)(void *mdata);
	void (*hv_afc_9v_charger)(void *mdata);
	void (*hv_qc_charger)(void *mdata);
	void (*hv_qc_5v_charger)(void *mdata);
	void (*hv_qc_9v_charger)(void *mdata);
	void (*hv_qc_failed)(void *mdata);
	int (*pm_chgin_irq)(void *mdata, int vol);
	int (*get_vbus_value)(void *mdata);
};

struct muic_ic_data {
	struct muic_ic_ops m_ops;
	void *drv_data;
};

struct muic_share_data {
	struct muic_platform_data *pdata;
	struct muic_interface_t *muic_if;
	struct muic_ic_data *ic_data;
	struct mutex hv_mutex;
	struct mutex attach_mutex;
	struct delayed_work hv_work;

	bool suspended;
	bool need_to_noti;

	bool is_factory_start;
	bool is_rustproof;
	bool is_new_factory;
	bool is_dcp_charger;
	bool is_afc_pdic_ready;
	bool is_afc_water_disable;
	bool is_pdic_attached;
	bool is_bypass;
	bool is_pdic_probe;
	bool is_jig_on;

	bool is_hiccup_mode;
	bool vbus_state;

	int gpio_uart_sel;
	int gpio_usb_sel;

	int hv_work_delay;

#if defined(CONFIG_MUIC_HV)
	muic_hv_state_t hv_state;
#endif

	muic_attached_dev_t attached_dev;
	muic_hv_voltage_t hv_voltage;

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
	struct power_supply *psy_muic;
	struct power_supply_desc psy_muic_desc;
#endif
};
#if IS_ENABLED(CONFIG_MUIC_PLATFORM)
extern int muic_platform_send_pdic_event(void *data, int event);
extern void muic_platform_handle_vbus(struct muic_share_data *sdata,
		bool vbus_on);
extern void muic_platform_handle_rid(struct muic_share_data *sdata,
		int new_dev);
extern int muic_platform_handle_attach(struct muic_share_data *sdata,
			muic_attached_dev_t new_dev);
extern int muic_platform_handle_detach(struct muic_share_data *sdata);
extern void muic_platform_handle_hv_attached_dev(struct muic_share_data *sdata,
			muic_attached_dev_t new_dev);
extern void muic_platform_send_hv_ready(struct muic_share_data *sdata);
#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
extern void hv_muic_change_afc_voltage(int tx_data);
#endif
extern int muic_platform_get_afc_disable(void *data);
extern int muic_platform_init_gpio_cb
		(struct muic_share_data *sdata);
extern int muic_platform_init_switch_dev
		(struct muic_share_data *sdata);
extern void muic_platform_deinit_switch_dev
		(struct muic_share_data *sdata);
extern int register_muic_platform_layer
				(struct muic_share_data *sdata);
extern void unregister_muic_platform_layer
				(struct muic_share_data *sdata);
extern bool muic_platform_get_pdic_cable_state
				(struct muic_share_data *sdata);
#if IS_ENABLED(CONFIG_MUIC_HV)
extern bool muic_platform_hv_is_hv_dev
				(struct muic_share_data *sdata);
extern int muic_platform_hv_state_manager(struct muic_share_data *sdata,
		muic_hv_transaction_t trans);
extern void muic_platform_hv_init
		(struct muic_share_data *sdata);
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
extern int muic_platform_psy_init(struct muic_share_data *sdata,
		struct device *parent);
#endif
#else
static inline int muic_platform_send_pdic_event(void *data, int event)
		{return 0; }
static inline void muic_platform_handle_vbus(struct muic_share_data *sdata,
		bool vbus_on) {}
static inline void muic_platform_handle_rid(struct muic_share_data *sdata,
		int new_dev) {}
static inline int muic_platform_handle_attach(struct muic_share_data *sdata,
			muic_attached_dev_t new_dev)
		{return 0; }
static inline int muic_platform_handle_detach(struct muic_share_data *sdata)
		{return 0; }
static inline void muic_platform_handle_hv_attached_dev(struct muic_share_data *sdata,
			muic_attached_dev_t new_dev) {}
static inline void muic_platform_send_hv_ready(struct muic_share_data *sdata) {}
#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
static inline void hv_muic_change_afc_voltage(int tx_data) {}
#endif
static inline int muic_platform_get_afc_disable(void *data)
		{return 0; }
static inline int muic_platform_init_gpio_cb
		(struct muic_share_data *sdata) {return 0; }
static inline int muic_platform_init_switch_dev
		(struct muic_share_data *sdata) {return 0; }
static inline void muic_platform_deinit_switch_dev
		(struct muic_share_data *sdata) {}
static inline int register_muic_platform_layer
		(struct muic_share_data *sdata) {return 0; }
static inline void unregister_muic_platform_layer
		(struct muic_share_data *sdata) {}
static inline bool muic_platform_get_pdic_cable_state
		(struct muic_share_data *sdata) {return 0; }
#if IS_ENABLED(CONFIG_MUIC_HV)
static inline bool muic_platform_hv_is_hv_dev
		(struct muic_share_data *sdata) {return 0; }
static inline int muic_platform_hv_state_manager(struct muic_share_data *sdata,
		muic_hv_transaction_t trans) {return 0; }
static inline void muic_platform_hv_init
		(struct muic_share_data *sdata) {}
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
static inline int muic_platform_psy_init(struct muic_share_data *sdata,
		struct device *parent) {return 0; }
#endif
#endif

#endif /* __MUIC_PLATFORM_LAYER_H__ */

