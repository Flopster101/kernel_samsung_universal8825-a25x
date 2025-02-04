#ifndef __SM5714_MUIC_H__
#define __SM5714_MUIC_H__
/*
 * Copyright (C) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/muic/common/muic.h>
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
#include <linux/usb/typec/manager/if_cb_manager.h>
#endif

#define MUIC_DEV_NAME	"muic-sm5714"


/* SM5714 muic register read/write related information defines. */

/* SM5714 Control register */
#define CTRL_nSW_OPEN_SHIFT		5
#define CTRL_DCDTIMER_SHIFT		3
#define CTRL_ENDCDTIMER_SHIFT	2
#define CTRL_BC12OFF_SHIFT		1
#define CTRL_BCD_RESCAN_SHIFT	0

#define CTRL_nSW_OPEN_MASK		(0x1 << CTRL_nSW_OPEN_SHIFT)
#define CTRL_DCDTIMER_MASK		(0x3 << CTRL_DCDTIMER_SHIFT)
#define CTRL_ENDCDTIMER_MASK	(0x1 << CTRL_ENDCDTIMER_SHIFT)
#define CTRL_BC12OFF_MASK		(0x1 << CTRL_BC12OFF_SHIFT)
#define CTRL_BCD_RESCAN_MASK	(0x1 << CTRL_BCD_RESCAN_SHIFT)

/* SM5714 MUIC Interrupt 1 register */
#define INT1_DPDM_OVP_SHIFT			5
#define INT1_VBUS_RID_DETACH_SHIFT	4
#define INT1_RID_DETECT_SHIFT		2
#define INT1_CHGTYPE_SHIFT			1
#define INT1_DCDTIMEOUT_SHIFT		0

#define INT1_DPDM_OVP_MASK			(0x1 << INT1_DPDM_OVP_SHIFT)
#define INT1_VBUS_RID_DETACH_MASK	(0x1 << INT1_VBUS_RID_DETACH_SHIFT)
#define INT1_RID_DETECT_MASK		(0x1 << INT1_RID_DETECT_SHIFT)
#define INT1_CHGTYPE_MASK			(0x1 << INT1_CHGTYPE_SHIFT)
#define INT1_DCDTIMEOUT_MASK		(0x1 << INT1_DCDTIMEOUT_SHIFT)

/* SM5714 MUIC Interrupt 2 register */
#define INT2_AFC_ERROR_SHIFT		5
#define INT2_AFC_STA_CHG_SHIFT		4
#define INT2_MULTI_BYTE_SHIFT		3
#define INT2_VBUS_UPDATE_SHIFT		2
#define INT2_AFC_ACCEPTED_SHIFT		1
#define INT2_AFC_TA_ATTACHED_SHIFT	0

#define INT2_AFC_ERROR_MASK			(0x1 << INT2_AFC_ERROR_SHIFT)
#define INT2_AFC_STA_CHG_MASK		(0x1 << INT2_AFC_STA_CHG_SHIFT)
#define INT2_MULTI_BYTE_MASK		(0x1 << INT2_MULTI_BYTE_SHIFT)
#define INT2_VBUS_UPDATE_MASK		(0x1 << INT2_VBUS_UPDATE_SHIFT)
#define INT2_AFC_ACCEPTED_MASK		(0x1 << INT2_AFC_ACCEPTED_SHIFT)
#define INT2_AFC_TA_ATTACHED_MASK	(0x1 << INT2_AFC_TA_ATTACHED_SHIFT)

#define SM5714_MUIC_IRQ_PROBE		(-1)
#define SM5714_MUIC_IRQ_WORK		(-2)

/* SM5714 MUIC Device Type 1 register */
#define DEV_TYPE1_LO_TA			(0x1 << 7)
#define DEV_TYPE1_QC20_TA		(0x1 << 6)
#define DEV_TYPE1_AFC_TA		(0x1 << 5)
#define DEV_TYPE1_U200			(0x1 << 4)
#define DEV_TYPE1_CDP			(0x1 << 3)
#define DEV_TYPE1_DCP			(0x1 << 2)
#define DEV_TYPE1_SDP			(0x1 << 1)
#define DEV_TYPE1_DCD_OUT_SDP	(0x1 << 0)

#define DEV_TYPE1_USB_TYPES		(DEV_TYPE1_CDP | DEV_TYPE1_SDP)
#define DEV_TYPE1_CHG_TYPES		(DEV_TYPE1_DCP | DEV_TYPE1_CDP \
							| DEV_TYPE1_U200)
#define DEV_TYPE1_AFC_DCP		(DEV_TYPE1_DCP | DEV_TYPE1_AFC_TA)
#define DEV_TYPE1_QC20_DCP		(DEV_TYPE1_DCP | DEV_TYPE1_QC20_TA)

/* SM5714 MUIC Device Type 2 register */
#define DEV_TYPE2_DEBUG_JTAG		(0x1 << 7)
#define DEV_TYPE2_HVDCP				(0x1 << 6)
#define DEV_TYPE2_JIG_UART_OFF		(0x1 << 3)
#define DEV_TYPE2_JIG_UART_ON		(0x1 << 2)
#define DEV_TYPE2_JIG_USB_OFF		(0x1 << 1)
#define DEV_TYPE2_JIG_USB_ON		(0x1 << 0)

#define DEV_TYPE2_JIG_USB_TYPES		(DEV_TYPE2_JIG_USB_OFF \
						| DEV_TYPE2_JIG_USB_ON)
#define DEV_TYPE2_JIG_UART_TYPES	(DEV_TYPE2_JIG_UART_OFF)
#define DEV_TYPE2_JIG_TYPES		(DEV_TYPE2_JIG_UART_TYPES \
						| DEV_TYPE2_JIG_USB_TYPES)

/* SM5714 AFC CTRL register */
#define AFCCTRL_QC20_9V			6
#define AFCCTRL_DIS_AFC			5
#define AFCCTRL_HVDCPTIMER		4
#define AFCCTRL_VBUS_READ		3
#define AFCCTRL_DM_RESET		2
#define AFCCTRL_DP_RESET		1
#define AFCCTRL_ENAFC			0

/*
 * Manual Switch
 * D- [5:3] / D+ [2:0]
 * 000: Open all / 001: USB / 010: JTGA / 011: UART / 100: GND
 */
#define MANUAL_SW_DM_SHIFT	3
#define MANUAL_SW_DP_SHIFT	0

#define MANUAL_SW_OPEN		(0x0)
#define MANUAL_SW_USB		(0x1 << MANUAL_SW_DM_SHIFT \
							| 0x1 << MANUAL_SW_DP_SHIFT)
#define MANUAL_SW_JTAG		(0x2 << MANUAL_SW_DM_SHIFT \
							| 0x2 << MANUAL_SW_DP_SHIFT)
#define MANUAL_SW_UART		(0x3 << MANUAL_SW_DM_SHIFT \
							| 0x3 << MANUAL_SW_DP_SHIFT)
#define MANUAL_SW_HICCUP	(0x4 << MANUAL_SW_DM_SHIFT \
							| 0x4 << MANUAL_SW_DP_SHIFT)

enum sm5714_reg_manual_sw_value {
	MANSW_OPEN = (MANUAL_SW_OPEN),
	MANSW_USB = (MANUAL_SW_USB),
	MANSW_JTAG = (MANUAL_SW_JTAG),
	MANSW_OTG = (MANUAL_SW_USB),
	MANSW_UART = (MANUAL_SW_UART),
	MANSW_HICCUP = (MANUAL_SW_HICCUP),
	MANSW_OPEN_RUSTPROOF = (MANUAL_SW_OPEN),
};

#define MANSW_AUTOMATIC		0
#define MANSW_MANUAL		1

#define AFC_TXBYTE_5V		0x0
#define AFC_TXBYTE_6V		0x1
#define AFC_TXBYTE_7V		0x2
#define AFC_TXBYTE_8V		0x3
#define AFC_TXBYTE_9V		0x4
#define AFC_TXBYTE_10V		0x5
#define AFC_TXBYTE_11V		0x6
#define AFC_TXBYTE_12V		0x7
#define AFC_TXBYTE_13V		0x8
#define AFC_TXBYTE_14V		0x9
#define AFC_TXBYTE_15V		0xA
#define AFC_TXBYTE_16V		0xB
#define AFC_TXBYTE_17V		0xC
#define AFC_TXBYTE_18V		0xD
#define AFC_TXBYTE_19V		0xE
#define AFC_TXBYTE_20V		0xF

#define AFC_TXBYTE_0_75A	0x0
#define AFC_TXBYTE_0_90A	0x1
#define AFC_TXBYTE_1_05A	0x2
#define AFC_TXBYTE_1_20A	0x3
#define AFC_TXBYTE_1_35A	0x4
#define AFC_TXBYTE_1_50A	0x5
#define AFC_TXBYTE_1_65A	0x6
#define AFC_TXBYTE_1_80A	0x7
#define AFC_TXBYTE_1_95A	0x8
#define AFC_TXBYTE_2_10A	0x9
#define AFC_TXBYTE_2_25A	0xA
#define AFC_TXBYTE_2_40A	0xB
#define AFC_TXBYTE_2_55A	0xC
#define AFC_TXBYTE_2_70A	0xD
#define AFC_TXBYTE_2_85A	0xE
#define AFC_TXBYTE_3_00A	0xF

#define AFC_TXBYTE_5V_1_95A		((AFC_TXBYTE_5V << 4) | AFC_TXBYTE_1_95A)
#define AFC_TXBYTE_9V_1_65A		((AFC_TXBYTE_9V << 4) | AFC_TXBYTE_1_65A)
#define AFC_TXBYTE_12V_2_10A	((AFC_TXBYTE_12V << 4) | AFC_TXBYTE_2_10A)

#define SM5714_MUIC_AFC_TA	0x00
#define SM5714_MUIC_QC20	0x01

#define SM5714_MUIC_HV_5V	0x08
#define SM5714_MUIC_HV_9V	0x46
#define SM5714_MUIC_HV_12V	0x79

#define SM5714_ENQC20_5V	0x0
#define SM5714_ENQC20_9V	0x1
#define SM5714_ENQC20_12V	0x2

#define SM5714_MUIC_REG_CFG1		0x31
#define SM5714_MUIC_REG_CFG2		0x32
#define SM5714_MUIC_REG_USBKS		0x38
#define SM5714_MUIC_REG_VBUS		0x3B
#define SM5714_MUIC_REG_AFC_RX_NUM	0x3D
#define SM5714_MUIC_REG_AFC_RX_P0	0x3E
#define SM5714_MUIC_REG_AFC_RX_P1	0x3F
#define SM5714_MUIC_REG_BCD_STATE	0x42
#define SM5714_MUIC_REG_AFC_STATE	0x43
#define SM5714_MUIC_REG_OTP_IF_STS	0x58

struct muic_irq_t {
	/* int1 */
	int irq_dpdm_ovp;
	int irq_vbus_rid_detach;
	int irq_rid_detect;
	int irq_chgtype_attach;
	int irq_dectimeout;
	/* int2 */
	int irq_afc_error;
	int irq_afc_sta_chg;
	int irq_multi_byte;
	int irq_afc_accepted;
	int irq_afc_ta_attached;
};

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
enum {
	SM5714_MUIC_AFC_NORMAL = 0,
	SM5714_MUIC_AFC_ABNORMAL = 1,
};

#define MUIC_PDIC_NOTI_ATTACH (1)
#define MUIC_PDIC_NOTI_DETACH (-1)
#define MUIC_PDIC_NOTI_UNDEFINED (0)

struct sm5714_pdic_evt {
	int pdic_evt_attached; /* 1: attached, -1: detached, 0: undefined */
	int pdic_evt_rid; /* the last rid */
	int pdic_evt_rprd; /*rprd */
	int pdic_evt_roleswap; /* check rprd role swap event */
	int pdic_evt_dcdcnt; /* count dcd timeout */
	int pdic_evt_abnormal_sbu_short_gender; /* sbu short gender */
};
#endif

struct sm5714_muic_data {

	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex muic_mutex;

	/* model dependant muic platform data */
	struct muic_platform_data *pdata;

	void *muic_data;

	/* muic current attached device */
	muic_attached_dev_t attached_dev;

	/*struct muic_intr_data intr;*/
	struct muic_irq_t irqs;

	int gpio_uart_sel;

	/* muic Device ID */
	u8 muic_vendor;			/* Vendor ID */
	u8 muic_version;		/* Version ID */

	bool is_factory_start;
	bool is_rustproof;
	bool is_otg_test;
	bool is_pdic_ready;
	bool is_charger_ready;

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
	/* USB Notifier */
	struct notifier_block	usb_nb;
#endif

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	struct sm5714_pdic_evt	pdic_info_data;
	int		pdic_evt_id;
	bool	need_to_path_open;
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct notifier_block	manager_nb;
#else
	struct notifier_block	pdic_nb;
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct notifier_block	vbus_nb;
#endif

	int pdic_afc_state;
	int pdic_afc_state_count;
	struct delayed_work	pdic_afc_work;
#endif

	struct mutex afc_mutex;
	struct sm5714_dev *sm5714_dev;

	/* model dependant mfd platform data */
	struct sm5714_platform_data	*mfd_pdata;

	int dev1;
	int dev2;

	struct wakeup_source *wake_lock;

	bool suspended;
	bool need_to_noti;

	bool is_water_detect;
	int vbus_state;

	/* AFC */
	bool afc_irq_disabled;
	int afc_retry_count;
	int vbus_changed_9to5;
	int fled_torch_enable;
	int fled_flash_enable;
	int hv_voltage;

	struct delayed_work	afc_retry_work;
	struct delayed_work	afc_torch_work;
	struct work_struct muic_afc_init_work;

	struct work_struct	muic_event_work;
	struct delayed_work	muic_debug_work;
	struct delayed_work	muic_U200_work;
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	struct muic_dev		*muic_d;
	struct if_cb_manager	*man;
#endif

#if defined(CONFIG_HICCUP_CHARGER)
	bool is_hiccup_mode;
#endif

#if defined(CONFIG_MUIC_BCD_RESCAN)
	int bc12_retry_count;
	int bc1p2_retry_count_max;
#endif

	struct delayed_work	delayed_523Kto619K_work;
	int delayed_523Kto619K_state;

	int shut_down;
};

extern struct device *switch_device;
extern unsigned int system_rev;
extern struct muic_platform_data muic_pdata;

int sm5714_i2c_read_byte(struct i2c_client *client, u8 command);
int sm5714_i2c_write_byte(struct i2c_client *client, u8 command, u8 value);

int muic_request_disable_afc_state(void);
int muic_check_fled_state(int enable, int mode);
int muic_disable_afc(int disable);
int sm5714_muic_voltage_control(struct sm5714_muic_data *muic_data, int afctxd, int qc20);

int sm5714_muic_afc_set_voltage(int vol);
int sm5714_muic_charger_init(void);
int sm5714_afc_ta_attach(struct sm5714_muic_data *muic_data);
int sm5714_afc_ta_accept(struct sm5714_muic_data *muic_data);
int sm5714_afc_multi_byte(struct sm5714_muic_data *muic_data);
int sm5714_afc_error(struct sm5714_muic_data *muic_data);
int sm5714_afc_sta_chg(struct sm5714_muic_data *muic_data);
int sm5714_muic_get_vbus_value(struct sm5714_muic_data *muic_data);

int sm5714_set_afc_ctrl_reg(struct sm5714_muic_data *muic_data,
		int shift, bool on);
void sm5714_hv_muic_initialize(struct sm5714_muic_data *muic_data);

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
void sm5714_muic_register_pdic_notifier(struct sm5714_muic_data *pmuic);
void sm5714_muic_unregister_pdic_notifier(struct sm5714_muic_data *pmuic);
#endif

#endif /* __SM5714_MUIC_H__ */
