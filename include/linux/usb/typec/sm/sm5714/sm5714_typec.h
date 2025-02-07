/*
 * Copyrights (C) 2018 Samsung Electronics, Inc.
 * Copyrights (C) 2018 Silicon Mitus, Inc.
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
 */

#ifndef __SM5714_TYPEC_H__
#define __SM5714_TYPEC_H__

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/common/pdic_core.h>
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/class-dual-role.h>
#elif defined(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
#include <linux/usb/typec/manager/if_cb_manager.h>
#endif
#include <linux/pm_wakeup.h>

#define USBPD_DEV_NAME					"usbpd-sm5714"

#define SM5714_MAX_NUM_MSG_OBJ				(7)

#define SM5714_REG_INT_STATUS1_VBUSPOK			(1<<0)
#define SM5714_REG_INT_STATUS1_TMR_EXP			(1<<1)
#define SM5714_REG_INT_STATUS1_ADC_DONE			(1<<2)
#define SM5714_REG_INT_STATUS1_ATTACH			(1<<3)
#define SM5714_REG_INT_STATUS1_DETACH			(1<<4)
#define SM5714_REG_INT_STATUS1_WAKEUP			(1<<5)
#define SM5714_REG_INT_STATUS1_ABNORMAL_DEV		(1<<7)

#define SM5714_REG_INT_STATUS2_PD_RID_DETECT	(1<<0)
#define SM5714_REG_INT_STATUS2_VCONN_DISCHG		(1<<2)
#define SM5714_REG_INT_STATUS2_AES_DONE			(1<<3)
#define SM5714_REG_INT_STATUS2_SRC_ADV_CHG		(1<<4)
#define SM5714_REG_INT_STATUS2_VBUS_0V			(1<<5)
#define SM5714_REG_INT_STATUS2_DEB_ORI_DETECT	(1<<6)

#define SM5714_REG_INT_STATUS3_WATER			(1<<0)
#define SM5714_REG_INT_STATUS3_CC1_OVP			(1<<1)
#define SM5714_REG_INT_STATUS3_CC2_OVP			(1<<2)
#define SM5714_REG_INT_STATUS3_VCONN_OCP		(1<<3)
#define SM5714_REG_INT_STATUS3_WATER_RLS		(1<<4)
#define SM5714_REG_INT_STATUS3_VCONN_OCL		(1<<5)
#define SM5714_REG_INT_STATUS3_SNKONLY_BAT		(1<<6)

#define SM5714_REG_INT_STATUS4_RX_DONE			(1<<0)
#define SM5714_REG_INT_STATUS4_TX_DONE			(1<<1)
#define SM5714_REG_INT_STATUS4_TX_SOP_ERR		(1<<2)
#define SM5714_REG_INT_STATUS4_PRL_RST_DONE		(1<<4)
#define SM5714_REG_INT_STATUS4_HRST_RCVED		(1<<5)
#define SM5714_REG_INT_STATUS4_HCRST_DONE		(1<<6)
#define SM5714_REG_INT_STATUS4_TX_DISCARD		(1<<7)

#define SM5714_REG_INT_STATUS5_SBU1_OVP			(1<<0)
#define SM5714_REG_INT_STATUS5_SBU2_OVP			(1<<1)
#define SM5714_REG_INT_STATUS5_JIG_CASE_ON		(1<<3)
#define SM5714_REG_INT_STATUS5_DPDM_SHORT		(1<<6)
#define SM5714_REG_INT_STATUS5_CC_ABNORMAL		(1<<7)

/* interrupt for checking message */
#define ENABLED_INT_1	(SM5714_REG_INT_STATUS1_VBUSPOK |\
			SM5714_REG_INT_STATUS1_TMR_EXP |\
			SM5714_REG_INT_STATUS1_ATTACH |\
			SM5714_REG_INT_STATUS1_DETACH |\
			SM5714_REG_INT_STATUS1_ABNORMAL_DEV)
#define ENABLED_INT_2	(SM5714_REG_INT_STATUS2_PD_RID_DETECT |\
			SM5714_REG_INT_STATUS2_VCONN_DISCHG|\
			SM5714_REG_INT_STATUS2_SRC_ADV_CHG|\
			SM5714_REG_INT_STATUS2_VBUS_0V)
#define ENABLED_INT_3	(SM5714_REG_INT_STATUS3_WATER |\
			SM5714_REG_INT_STATUS3_CC1_OVP |\
			SM5714_REG_INT_STATUS3_CC2_OVP |\
			SM5714_REG_INT_STATUS3_VCONN_OCP |\
			SM5714_REG_INT_STATUS3_WATER_RLS|\
			SM5714_REG_INT_STATUS3_VCONN_OCL)
#define ENABLED_INT_4	(SM5714_REG_INT_STATUS4_RX_DONE |\
			SM5714_REG_INT_STATUS4_TX_DONE |\
			SM5714_REG_INT_STATUS4_TX_SOP_ERR |\
			SM5714_REG_INT_STATUS4_PRL_RST_DONE |\
			SM5714_REG_INT_STATUS4_HRST_RCVED |\
			SM5714_REG_INT_STATUS4_HCRST_DONE |\
			SM5714_REG_INT_STATUS4_TX_DISCARD)
#define ENABLED_INT_5	(SM5714_REG_INT_STATUS5_SBU1_OVP |\
			SM5714_REG_INT_STATUS5_SBU2_OVP |\
			SM5714_REG_INT_STATUS5_CC_ABNORMAL)

#define SM5714_ATTACH_SOURCE				0x01
#define SM5714_ATTACH_SINK				(0x01 << SM5714_ATTACH_SOURCE)
#define SM5714_ATTACH_AUDIO				0x03
#define SM5714_ATTACH_UN_ORI_DEBUG_SOURCE		(0x01 << SM5714_ATTACH_SINK)
#define SM5714_ATTACH_ORI_DEBUG_SOURCE			0x05
#define SM5714_ATTACH_TYPE				0x07
#define SM5714_ADV_CURR					0x18
#define SM5714_CABLE_FLIP				0x20
#define SM5714_REG_CNTL_NOTIFY_RESET_DONE		SM5714_ATTACH_SOURCE
#define SM5714_REG_CNTL_CABLE_RESET_MESSAGE		(SM5714_REG_CNTL_NOTIFY_RESET_DONE << 1)
#define SM5714_REG_CNTL_HARD_RESET_MESSAGE		(SM5714_REG_CNTL_NOTIFY_RESET_DONE << 2)
#define SM5714_REG_CNTL_PROTOCOL_RESET_MESSAGE	(SM5714_REG_CNTL_NOTIFY_RESET_DONE << 3)
#define SM5714_REG_CNTL_START_AMS_PROTOCOL		(SM5714_REG_CNTL_NOTIFY_RESET_DONE << 4)
#define SM5714_REG_CNTL_END_AMS_PROTOCOL		(SM5714_REG_CNTL_NOTIFY_RESET_DONE << 5)
#define SM5714_REG_CNTL_RP_AUTO_CONTRL_ENABLE	(SM5714_REG_CNTL_NOTIFY_RESET_DONE << 7)

#define SM5714_SBU_CORR_CHECK				(1<<6)
#define SM5714_DEAD_RD_ENABLE				(1<<7)

#define SM5714_REG_VBUS_DISCHG_CRTL_SHIFT	(5)
#define SM5714_REG_CNTL_VBUS_DISCHG_OFF \
		(0x0 << SM5714_REG_VBUS_DISCHG_CRTL_SHIFT) /* 0x00 */
#define SM5714_REG_CNTL_VBUS_DISCHG_ON \
		(0x1 << SM5714_REG_VBUS_DISCHG_CRTL_SHIFT) /* 0x20 */

#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT)
#define SM5714_SBU_OFF_THRESHOLD			0x10
#define SM5714_SBU_ON_THRESHOLD				0x80
#else
#define SM5714_SBU_OFF_THRESHOLD			0x24
#define SM5714_SBU_ON_THRESHOLD				0x3E
#endif

#define SM5714_ADC_PATH_SEL_CC1				0x01
#define SM5714_ADC_PATH_SEL_CC2				0x03
#define SM5714_ADC_PATH_SEL_VBUS			0x07
#define SM5714_ADC_PATH_SEL_SBU1			0x09
#define SM5714_ADC_PATH_SEL_SBU2			0x0B
#define SM5714_ADC_DONE						0x80

/* For SM5714_REG_TX_REQ_MESSAGE */
#define SM5714_REG_MSG_SEND_TX_SOP_REQ			0x07
#define SM5714_REG_MSG_SEND_TX_SOPP_REQ			0x17
#define SM5714_REG_MSG_SEND_TX_SOPPP_REQ		0x27

#define DATA_ROLE_SWAP 1
#define POWER_ROLE_SWAP 2
#define SM5714_CABLE_TYPE_SHIFT	6

enum sm5714_power_role {
	PDIC_SINK,
	PDIC_SOURCE
};

enum sm5714_pdic_rid {
	REG_RID_UNDF = 0x00,
	REG_RID_255K = 0x03,
	REG_RID_301K = 0x04,
	REG_RID_523K = 0x05,
	REG_RID_619K = 0x06,
	REG_RID_OPEN = 0x07,
	REG_RID_MAX  = 0x08,
};

/* SM5714 I2C registers */
enum sm5714_usbpd_reg {
	SM5714_REG_INT1				= 0x01,
	SM5714_REG_INT2 			= 0x02,
	SM5714_REG_INT3 			= 0x03,
	SM5714_REG_INT4				= 0x04,
	SM5714_REG_INT5 			= 0x05,
	SM5714_REG_INT_MASK1		= 0x06,
	SM5714_REG_INT_MASK2		= 0x07,
	SM5714_REG_INT_MASK3		= 0x08,
	SM5714_REG_INT_MASK4		= 0x09,
	SM5714_REG_INT_MASK5		= 0x0A,
	SM5714_REG_STATUS1			= 0x0B,
	SM5714_REG_STATUS2			= 0x0C,
	SM5714_REG_STATUS3			= 0x0D,
	SM5714_REG_STATUS4			= 0x0E,
	SM5714_REG_STATUS5			= 0x0F,
	SM5714_REG_JIGON_CONTROL	= 0x17,
	SM5714_REG_FACTORY			= 0x18,
	SM5714_REG_ADC_CNTL1		= 0x19,
	SM5714_REG_ADC_CNTL2		= 0x1A,
	SM5714_REG_SYS_CNTL			= 0x1B,
	SM5714_REG_COMP_CNTL		= 0x1C,
	SM5714_REG_CLK_CNTL			= 0x1D,
	SM5714_REG_USBK_CNTL 		= 0x1E,
	SM5714_REG_CORR_CNTL1		= 0x20,
	SM5714_REG_CORR_CNTL4		= 0x23,
	SM5714_REG_CORR_CNTL5		= 0x24,
	SM5714_REG_CORR_CNTL6		= 0x25,
	SM5714_REG_CC_STATUS		= 0x28,
	SM5714_REG_CC_CNTL1			= 0x29,
	SM5714_REG_CC_CNTL2			= 0x2A,
	SM5714_REG_CC_CNTL3			= 0x2B,
	SM5714_REG_CC_CNTL4			= 0x2C,
	SM5714_REG_CC_CNTL5			= 0x2D,
	SM5714_REG_CC_CNTL6			= 0x2E,
	SM5714_REG_CC_CNTL7			= 0x2F,
	SM5714_REG_CABLE_POL_SEL	= 0x33,
	SM5714_REG_GEN_TMR_L		= 0x35,
	SM5714_REG_GEN_TMR_U		= 0x36,
	SM5714_REG_PD_CNTL1			= 0x38,
	SM5714_REG_PD_CNTL2			= 0x39,
	SM5714_REG_PD_CNTL4			= 0x3B,
	SM5714_REG_PD_CNTL5 		= 0x3C,
	SM5714_REG_RX_SRC			= 0x41,
	SM5714_REG_RX_HEADER_00		= 0x42,
	SM5714_REG_RX_HEADER_01		= 0x43,
	SM5714_REG_RX_PAYLOAD		= 0x44,
	SM5714_REG_RX_BUF			= 0x5E,
	SM5714_REG_RX_BUF_ST		= 0x5F,
	SM5714_REG_TX_HEADER_00		= 0x60,
	SM5714_REG_TX_HEADER_01		= 0x61,
	SM5714_REG_TX_PAYLOAD		= 0x62,
	SM5714_REG_TX_BUF_CTRL		= 0x63,
	SM5714_REG_TX_REQ			= 0x7E,
	SM5714_REG_BC12_DEV_TYPE	= 0x88,
	SM5714_REG_TA_STATUS		= 0x89,
	SM5714_REG_CORR_CNTL9		= 0x92,
	SM5714_REG_CORR_TH3			= 0xA4,
	SM5714_REG_CORR_TH6			= 0xA7,
	SM5714_REG_CORR_TH7			= 0xA8,
	SM5714_REG_CORR_OPT4		= 0xC8,
	SM5714_REG_PROBE0			= 0xD0,
	SM5714_REG_PD_STATE0		= 0xD5,
	SM5714_REG_PD_STATE1		= 0xD6,
	SM5714_REG_PD_STATE2		= 0xD7,
	SM5714_REG_PD_STATE3		= 0xD8,
	SM5714_REG_PD_STATE4		= 0xD9,
	SM5714_REG_PD_STATE5		= 0xDA
};

typedef enum {
	AUTO_RP_CNTL = 0,
	END_AMS_PRL = 1,
	STR_AMS_PRL = 2,
} PDIC_AMS_MODE;

typedef enum {
	NON_PWR_CABLE = 0,
	PWR_CABLE = 1,
} PDIC_CABLE_TYPE;

typedef enum {
	PLUG_CTRL_RP0 = 0,
	PLUG_CTRL_RP80 = 1,
	PLUG_CTRL_RP180 = 2,
	PLUG_CTRL_RP330 = 3
} PDIC_RP_SCR_SEL;

typedef enum {
	PD_DISABLE = 0,
	PD_ENABLE = 1,
} PD_FUNC_MODE;

typedef enum {
	SBU_SOURCING_OFF = 0,
	SBU_SOURCING_ON = 1,
} ADC_REQUEST_MODE;

typedef enum {
	WATER_MODE_OFF = 0,
	WATER_MODE_ON = 1,
} PDIC_WATER_MODE;

#if defined(CONFIG_SEC_FACTORY)
#define FAC_ABNORMAL_REPEAT_STATE			12
#define FAC_ABNORMAL_REPEAT_RID				5
#define FAC_ABNORMAL_REPEAT_RID0			3
struct AP_REQ_GET_STATUS_Type {
	uint32_t FAC_Abnormal_Repeat_State;
	uint32_t FAC_Abnormal_Repeat_RID;
	uint32_t FAC_Abnormal_RID0;
};
#endif

struct sm5714_phydrv_data {
	struct device *dev;
	struct i2c_client *i2c;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	ppdic_data_t ppdic_data;
	struct workqueue_struct *pdic_wq;
#endif
	struct mutex _mutex;
	struct mutex poll_mutex;
	struct mutex lpm_mutex;
	struct mutex i2c_lock;
	int vconn_en;
	int irq_gpio;
	int irq;
	int vbus_dischg_gpio;
	int otg_det_gpio;
	int power_role;
	int data_role;
	int vconn_source;
	int scr_sel;
	msg_header_type header;
	data_obj_type obj[SM5714_MAX_NUM_MSG_OBJ];
	u64 status_reg;
	bool lpm_mode;
	bool detach_valid;
	bool is_cc_abnormal_state;
#if defined(CONFIG_SM5714_SUPPORT_SBU)
	bool is_sbu_abnormal_state;
#endif
	bool is_factory_mode;
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	bool is_water_detect;
#endif
	bool is_otg_vboost;
	bool is_jig_case_on;
	bool is_noti_src_adv;
	bool is_lpcharge;
	bool is_1st_short;
	bool is_2nd_short;
	bool is_sbu_vbus_short;
	bool is_mpsm_exit;
	bool suspended;
	bool soft_reset;
	bool is_keystring;
	bool is_timer_expired;
	bool is_wait_sinktxok;
	wait_queue_head_t suspend_wait;
	struct wakeup_source	*irq_ws;
	int cc_open_cmd;
	int check_msg_pass;
	int rid;
	int is_attached;
	int reset_done;
	int pd_support;
	int abnormal_dev_cnt;
	int rp_currentlvl;
	struct delayed_work role_swap_work;
	struct delayed_work usb_external_notifier_register_work;
	struct notifier_block usb_external_notifier_nb;
	struct completion exit_mpsm_completion;
#if defined(CONFIG_SEC_FACTORY)
	struct AP_REQ_GET_STATUS_Type factory_mode;
	struct delayed_work factory_state_work;
	struct delayed_work factory_rid_work;
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	struct dual_role_phy_instance *dual_role;
	struct dual_role_phy_desc *desc;
	struct completion reverse_completion;
	int data_role_dual;
	int power_role_dual;
	int try_state_change;
#elif defined(CONFIG_TYPEC)
	struct typec_port *port;
	struct typec_partner *partner;
	struct usb_pd_identity partner_identity;
	struct typec_capability typec_cap;
	struct completion typec_reverse_completion;
	int typec_power_role;
	int typec_data_role;
	int typec_try_state_change;
	int pwr_opmode;
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct delayed_work vbus_noti_work;
#endif
	int vbus_noti_status;
	struct delayed_work muic_noti_work;
	struct delayed_work rx_buf_work;
	struct delayed_work vbus_dischg_work;
	struct delayed_work otg_det_work;
	struct delayed_work debug_work;
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	struct usbpd_dev	*usbpd_d;
	struct if_cb_manager	*man;
#endif
	wait_queue_head_t host_turn_on_wait_q;
	int host_turn_on_event;
	int host_turn_on_wait_time;
	int detach_done_wait;
	int wait_entermode;
	int shut_down;
};

extern struct sm5714_usbpd_data *sm5714_g_pd_data;

void sm5714_JIGON(void *data, bool mode);
void sm5714_usbpd_set_usb_safe_mode(void *_data);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
void sm5714_protocol_layer_reset(void *_data);
void sm5714_cc_state_hold_on_off(void *_data, int onoff);
bool sm5714_check_vbus_state(void *_data);
void select_pdo(int num);
void sm5714_pdic_event_work(void *data, int dest, int id, int attach, int event, int sub);
#endif
#if defined(CONFIG_TYPEC)
int sm5714_get_pd_support(struct sm5714_phydrv_data *usbpd_data);
#endif
#if defined(CONFIG_SM5714_SUPPORT_SBU)
void sm5714_short_state_check(void *_data);
#endif
void sm5714_cc_control_command(void *data, int is_off);
void sm5714_set_enable_pd_function(void *_data, int enable);
#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT) && defined(CONFIG_SM5714_SUPPORT_SBU)
void sm5714_usbpd_delayed_sbu_short_notify(void *_data);
#endif
void sm5714_vbus_turn_on_ctrl(struct sm5714_phydrv_data *usbpd_data, bool enable);
void sm5714_src_transition_to_default(void *_data);
void sm5714_src_transition_to_pwr_on(void *_data);
void sm5714_snk_transition_to_default(void *_data);
bool sm5714_get_rx_buf_st(void *_data);
void sm5714_set_bist_carrier_m2(void *_data);
void sm5714_usbpd_set_vbus_dischg_gpio(struct sm5714_phydrv_data *pdic_data, int vbus_dischg);
void sm5714_error_recovery_mode(void *_data);
void sm5714_detach_with_cc(int state);
#endif /* __SM5714_TYPEC_H__ */
