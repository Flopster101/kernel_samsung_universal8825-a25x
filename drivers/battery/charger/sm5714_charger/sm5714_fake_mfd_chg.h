/*
 * sm5714_fake_mfd_chg.h - header file of SM5714 Charger device driver
 *
 * Copyright (C) 2023 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __sm5714_FAKE_MFD_CHG_H
#define __sm5714_FAKE_MFD_CHG_H __FILE__
/* Secondary addr	= 0x92 : SW Charger, RGB, FLED */
enum sm5714_charger_reg {
	/* SW Charger */
	SM5714_CHG_REG_INT_SOURCE		= 0x00,
	SM5714_CHG_REG_INT1				= 0x01,
	SM5714_CHG_REG_INT2				= 0x02,
	SM5714_CHG_REG_INT3				= 0x03,
	SM5714_CHG_REG_INT4				= 0x04,
	SM5714_CHG_REG_INT5				= 0x05,
	SM5714_CHG_REG_INTMSK1			= 0x07,
	SM5714_CHG_REG_INTMSK2			= 0x08,
	SM5714_CHG_REG_INTMSK3			= 0x09,
	SM5714_CHG_REG_INTMSK4			= 0x0A,
	SM5714_CHG_REG_INTMSK5			= 0x0B,
	SM5714_CHG_REG_STATUS1			= 0x0D,
	SM5714_CHG_REG_STATUS2			= 0x0E,
	SM5714_CHG_REG_STATUS3			= 0x0F,
	SM5714_CHG_REG_STATUS4			= 0x10,
	SM5714_CHG_REG_STATUS5			= 0x11,
	SM5714_CHG_REG_CNTL1			= 0x13,
	SM5714_CHG_REG_CNTL2			= 0x14,
	SM5714_CHG_REG_VBUSCNTL			= 0x15,
	SM5714_CHG_REG_CHGCNTL1			= 0x17,
	SM5714_CHG_REG_CHGCNTL2			= 0x18,
	SM5714_CHG_REG_CHGCNTL3			= 0x19,
	SM5714_CHG_REG_CHGCNTL4			= 0x1A,
	SM5714_CHG_REG_CHGCNTL5			= 0x1B,
	SM5714_CHG_REG_CHGCNTL6			= 0x1C,
	SM5714_CHG_REG_CHGCNTL7			= 0x1D,
	SM5714_CHG_REG_CHGCNTL8			= 0x1E,
	SM5714_CHG_REG_WDTCNTL			= 0x22,
	SM5714_CHG_REG_BSTCNTL1			= 0x23,
	SM5714_CHG_REG_FACTORY1			= 0x25,
	SM5714_CHG_REG_FACTORY2			= 0x26,
	SM5714_CHG_REG_SINKADJ			= 0x40,
	SM5714_CHG_REG_FLEDCNTL1		= 0x41,
	SM5714_CHG_REG_FLEDCNTL2		= 0x42,
	SM5714_CHG_REG_CHGCNTL11		= 0x46,
	SM5714_CHG_REG_PRODUCTID1		= 0x4E,
	SM5714_CHG_REG_PRODUCTID2		= 0x4F,
	SM5714_CHG_REG_DEVICEID			= 0x50,

	SM5714_CHG_REG_END,
};

enum sm5714_irq {
	/* CHG INT1 */
	SM5714_CHG_IRQ_INT1_VBUSLIMIT	= 0,
	SM5714_CHG_IRQ_INT1_VBUSOVP,
	SM5714_CHG_IRQ_INT1_VBUSUVLO,
	SM5714_CHG_IRQ_INT1_VBUSPOK,
	/* CHG INT2 */
	SM5714_CHG_IRQ_INT2_WDTMROFF,
	SM5714_CHG_IRQ_INT2_DONE,
	SM5714_CHG_IRQ_INT2_TOPOFF,
	SM5714_CHG_IRQ_INT2_Q4FULLON,
	SM5714_CHG_IRQ_INT2_CHGON,
	SM5714_CHG_IRQ_INT2_NOBAT,
	SM5714_CHG_IRQ_INT2_BATOVP,
	SM5714_CHG_IRQ_INT2_AICL,
	/* CHG INT3 */
	SM5714_CHG_IRQ_INT3_VSYSOVP,
	SM5714_CHG_IRQ_INT3_nENQ4,
	SM5714_CHG_IRQ_INT3_FASTTMROFF,
	SM5714_CHG_IRQ_INT3_TRICKLETMROFF,
	SM5714_CHG_IRQ_INT3_DISLIMIT,
	SM5714_CHG_IRQ_INT3_OTGFAIL,
	SM5714_CHG_IRQ_INT3_THEMSHDN,
	SM5714_CHG_IRQ_INT3_THEMREG,
	/* CHG INT4 */
	SM5714_CHG_IRQ_INT4_CVMODE,
	SM5714_CHG_IRQ_INT4_BOOSTPOK,
	SM5714_CHG_IRQ_INT4_BOOSTPOK_NG,
	/* CHG INT5 */
	SM5714_CHG_IRQ_INT5_ABSTMROFF,
	SM5714_CHG_IRQ_INT5_FLEDOPEN,
	SM5714_CHG_IRQ_INT5_FLEDSHORT,
};

enum sm5714_irq_source {
	CHG_INT1 = 0,
	CHG_INT2,
	CHG_INT3,
	CHG_INT4,
	CHG_INT5,
	FG_INT,
	SM5714_IRQ_GROUP_NR,
};

/* support SM5714 Charger operation mode control module */
enum {
	SM5714_CHARGER_OP_EVENT_VBUSIN		= 0x5,
	SM5714_CHARGER_OP_EVENT_USB_OTG		= 0x4,
	SM5714_CHARGER_OP_EVENT_PWR_SHAR	= 0x3,
	SM5714_CHARGER_OP_EVENT_FLASH		= 0x2,
	SM5714_CHARGER_OP_EVENT_TORCH		= 0x1,
	SM5714_CHARGER_OP_EVENT_SUSPEND		= 0x0,
};

struct sm5714_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;

	struct mfd_cell *sub_devices;
	int num_subdevs;
};

struct sm5714_dev {
	struct device *dev;
	struct i2c_client *charger;	/* 0x92; Charger */
	struct i2c_client *fuelgauge;	/* 0xE2; Fuelgauge */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	bool suspended;
	wait_queue_head_t suspend_wait;
	struct wakeup_source	*irq_ws;
	struct mutex irqlock;
	int irq_masks_cur[SM5714_IRQ_GROUP_NR];
	int irq_masks_cache[SM5714_IRQ_GROUP_NR];

	/* For IC-Reset protection */
	void (*check_chg_reset)(struct i2c_client *, void *);
	void (*check_fg_reset)(struct i2c_client *, void *);
	void *chg_data;
	void *fg_data;

	u8 pmic_rev;
	u8 vender_id;

	struct sm5714_platform_data *pdata;
};

static inline int sm5714_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{ *dest = 0; return 0; }
static inline int sm5714_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{ return 0; }
static inline int sm5714_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{ return 0; }
static inline int sm5714_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{ return 0; }

extern int sm5714_charger_oper_table_init(struct sm5714_dev *sm5714);
extern int sm5714_charger_oper_push_event(int event_type, bool enable);
extern int sm5714_charger_oper_get_current_status(void);
extern int sm5714_charger_oper_get_current_op_mode(void);
extern int sm5714_charger_oper_en_factory_mode(int dev_type, int rid, bool enable);
extern int sm5714_charger_oper_forced_vbus_limit_control(int mA);

#endif /* __sm5714_FAKE_MFD_CHG_H */
