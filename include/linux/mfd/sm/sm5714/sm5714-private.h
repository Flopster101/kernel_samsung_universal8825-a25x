/*
 *  sm5714-private.h - IF-PMIC device driver for SM5714
 *
 *  Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5714_PRIV_H__
#define __SM5714_PRIV_H__

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>

#define SM5714_I2C_SADR_MUIC	(0x4A >> 1)
#define SM5714_I2C_SADR_CHG     (0x92 >> 1)
#define SM5714_I2C_SADR_FG      (0xE2 >> 1)

#define SM5714_IRQSRC_MUIC	    (1 << 0)
#define SM5714_IRQSRC_CHG	    (1 << 1)
#define SM5714_IRQSRC_FG        (1 << 2)
#define SM5714_REG_INVALID		(0xffff)


/* Slave addr = 0x4A : MUIC */
enum sm5714_muic_reg {
    SM5714_MUIC_REG_DeviceID        = 0x00,
    SM5714_MUIC_REG_INT1            = 0x01,
    SM5714_MUIC_REG_INT2            = 0x02,
    SM5714_MUIC_REG_INTMASK1        = 0x03,
    SM5714_MUIC_REG_INTMASK2        = 0x04,
    SM5714_MUIC_REG_CNTL            = 0x05,
    SM5714_MUIC_REG_MANUAL_SW       = 0x06,
    SM5714_MUIC_REG_DEVICETYPE1     = 0x07,
    SM5714_MUIC_REG_DEVICETYPE2     = 0x08,
    SM5714_MUIC_REG_AFCCNTL         = 0x09,
    SM5714_MUIC_REG_AFCTXD          = 0x0A,
    SM5714_MUIC_REG_AFCSTATUS       = 0x0B,
    SM5714_MUIC_REG_VBUS_VOLTAGE    = 0x0C,
    SM5714_MUIC_REG_AFC_RXD1        = 0x0E,
    SM5714_MUIC_REG_AFC_RXD2        = 0x0F,
    SM5714_MUIC_REG_AFC_RXD3        = 0x10,
    SM5714_MUIC_REG_AFC_RXD4        = 0x11,
    SM5714_MUIC_REG_AFC_RXD5        = 0x12,
    SM5714_MUIC_REG_AFC_RXD6        = 0x13,
    SM5714_MUIC_REG_AFC_RXD7        = 0x14,
    SM5714_MUIC_REG_AFC_RXD8        = 0x15,
    SM5714_MUIC_REG_AFC_RXD9        = 0x16,
    SM5714_MUIC_REG_AFC_RXD10       = 0x17,
    SM5714_MUIC_REG_AFC_RXD11       = 0x18,
    SM5714_MUIC_REG_AFC_RXD12       = 0x19,
    SM5714_MUIC_REG_AFC_RXD13       = 0x1A,
    SM5714_MUIC_REG_AFC_RXD14       = 0x1B,
    SM5714_MUIC_REG_AFC_RXD15       = 0x1C,
    SM5714_MUIC_REG_AFC_RXD16       = 0x1D,
    SM5714_MUIC_REG_RESET           = 0x1E,

    SM5714_MUIC_REG_END,
};

/* Slave addr = 0x92 : SW Charger, RGB, FLED */
enum sm5714_charger_reg {
    /* SW Charger */
    SM5714_CHG_REG_INT_SOURCE       = 0x00,
    SM5714_CHG_REG_INT1             = 0x01,
    SM5714_CHG_REG_INT2             = 0x02,
    SM5714_CHG_REG_INT3             = 0x03,
    SM5714_CHG_REG_INT4             = 0x04,
    SM5714_CHG_REG_INT5             = 0x05,
    SM5714_CHG_REG_INTMSK1          = 0x07,
    SM5714_CHG_REG_INTMSK2          = 0x08,
    SM5714_CHG_REG_INTMSK3          = 0x09,
    SM5714_CHG_REG_INTMSK4          = 0x0A,
    SM5714_CHG_REG_INTMSK5          = 0x0B,
    SM5714_CHG_REG_STATUS1          = 0x0D,
    SM5714_CHG_REG_STATUS2          = 0x0E,
    SM5714_CHG_REG_STATUS3          = 0x0F,
    SM5714_CHG_REG_STATUS4          = 0x10,
    SM5714_CHG_REG_STATUS5          = 0x11,
    SM5714_CHG_REG_CNTL1            = 0x13,
    SM5714_CHG_REG_CNTL2            = 0x14,
    SM5714_CHG_REG_VBUSCNTL         = 0x15,
    SM5714_CHG_REG_CHGCNTL1         = 0x17,
    SM5714_CHG_REG_CHGCNTL2         = 0x18,
	SM5714_CHG_REG_CHGCNTL3         = 0x19,
    SM5714_CHG_REG_CHGCNTL4         = 0x1A,
    SM5714_CHG_REG_CHGCNTL5         = 0x1B,
    SM5714_CHG_REG_CHGCNTL6         = 0x1C,
    SM5714_CHG_REG_CHGCNTL7         = 0x1D,
    SM5714_CHG_REG_CHGCNTL8         = 0x1E,
    SM5714_CHG_REG_WDTCNTL          = 0x22,
    SM5714_CHG_REG_BSTCNTL1         = 0x23,
    SM5714_CHG_REG_FACTORY1         = 0x25,
    SM5714_CHG_REG_FACTORY2         = 0x26,
    SM5714_CHG_REG_SINKADJ          = 0x40,
    SM5714_CHG_REG_FLEDCNTL1        = 0x41,
    SM5714_CHG_REG_FLEDCNTL2        = 0x42,
    SM5714_CHG_REG_CHGCNTL11        = 0x46,
    SM5714_CHG_REG_PRODUCTID1       = 0x4E,
    SM5714_CHG_REG_PRODUCTID2       = 0x4F,
    SM5714_CHG_REG_DEVICEID         = 0x50,

    SM5714_CHG_REG_END,
};

/* Slave addr = 0xE2 : FUEL GAUGE */
enum sm5714_fuelgauge_reg {
    SM5714_FG_REG_DEVICE_ID         = 0x00,
	SM5714_FG_REG_CTRL				= 0x01,
    SM5714_FG_REG_INTFG             = 0x02,
    SM5714_FG_REG_STATUS            = 0x03,
    SM5714_FG_REG_INTFG_MASK        = 0x04,

	SM5714_FG_REG_SYSTEM_STATUS		= 0x10,
    SM5714_FG_REG_TABLE_UNLOCK		= 0x13,

	SM5714_FG_REG_AUX_CTRL1			= 0x20,
	SM5714_FG_REG_AUX_CTRL2			= 0x21,
	SM5714_FG_REG_AUX_STAT			= 0x22,

	SM5714_FG_REG_SRAM_PROT		    = 0x8B,
	SM5714_FG_REG_SRAM_RADDR		= 0x8C,
	SM5714_FG_REG_SRAM_RDATA		= 0x8D,
	SM5714_FG_REG_SRAM_WADDR		= 0x8E,
	SM5714_FG_REG_SRAM_WDATA		= 0x8F,

    SM5714_FG_REG_RESET             = 0x91,

	//SRAM ADDR
	SM5714_FG_ADDR_SRAM_SOC			= 0x00,
	SM5714_FG_ADDR_SRAM_OCV			= 0x01,
	SM5714_FG_ADDR_SRAM_VBAT		= 0x03,
	SM5714_FG_ADDR_SRAM_VSYS		= 0x04,
	SM5714_FG_ADDR_SRAM_CURRENT		= 0x05,
	SM5714_FG_ADDR_SRAM_TEMPERATURE	= 0x07,
	SM5714_FG_ADDR_SRAM_VBAT_AVG	= 0x08,
	SM5714_FG_ADDR_SRAM_CURRENT_AVG	= 0x09,
	SM5714_FG_ADDR_SRAM_STATE       = 0x15,

    SM5714_FG_ADDR_SRAM_START_LB_V  = 0x20,
    SM5714_FG_ADDR_SRAM_START_CB_V  = 0x28,
    SM5714_FG_ADDR_SRAM_START_LB_I  = 0x30,
    SM5714_FG_ADDR_SRAM_START_CB_I  = 0x38,



	SM5714_FG_ADDR_SRAM_AGING_RATE_FILT = 0x46,


	SM5714_FG_ADDR_SRAM_VOFFSET     = 0x61,
    SM5714_FG_ADDR_SRAM_VSLOPE      = 0x62,

	SM5714_FG_ADDR_SRAM_DP_IOFFSET  = 0x63,
    SM5714_FG_ADDR_SRAM_DP_IPSLOPE  = 0x64,
	SM5714_FG_ADDR_SRAM_DP_INSLOPE  = 0x65,
    SM5714_FG_ADDR_SRAM_ALG_IOFFSET = 0x66,
    SM5714_FG_ADDR_SRAM_ALG_IPSLOPE = 0x67,
    SM5714_FG_ADDR_SRAM_ALG_INSLOPE = 0x68,

	SM5714_FG_ADDR_SRAM_VVT         = 0x6C,
    SM5714_FG_ADDR_SRAM_IVT         = 0x6D,
    SM5714_FG_ADDR_SRAM_IVV         = 0x6E,

	SM5714_FG_ADDR_SRAM_RS_MIN      = 0x73,
	SM5714_FG_ADDR_SRAM_RS_MAX      = 0x74,
	SM5714_FG_ADDR_SRAM_RS_FACTOR   = 0x75,
    SM5714_FG_ADDR_SRAM_RS_CHG_FACTOR   = 0x76,
    SM5714_FG_ADDR_SRAM_RS_DISCHG_FACTOR   = 0x77,
	SM5714_FG_ADDR_SRAM_RS_AUTO_MAN_VALUE = 0x78,
	SM5714_FG_ADDR_SRAM_Q_MAX		= 0x79,

    SM5714_FG_ADDR_SRAM_INIT_OCV    = 0x7A,

	SM5714_FG_ADDR_SRAM_INTR_VL		= 0x80,
    SM5714_FG_ADDR_SRAM_INTR_VL_HYS = 0x81,

	SM5714_FG_ADDR_SRAM_SOC_CYCLE	= 0x87,

	SM5714_FG_ADDR_SRAM_USER_RESERV_1	= 0x8A,
    SM5714_FG_ADDR_SRAM_USER_RESERV_2   = 0x8B,

	SM5714_FG_ADDR_TABLE0_0			= 0x90,
	SM5714_FG_ADDR_TABLE0_1			= 0x91,
	SM5714_FG_ADDR_TABLE0_2			= 0x92,
	SM5714_FG_ADDR_TABLE0_3			= 0x93,
	SM5714_FG_ADDR_TABLE0_4			= 0x94,
	SM5714_FG_ADDR_TABLE0_5			= 0x95,
	SM5714_FG_ADDR_TABLE0_6			= 0x96,
	SM5714_FG_ADDR_TABLE0_7			= 0x97,
	SM5714_FG_ADDR_TABLE0_8			= 0x98,
	SM5714_FG_ADDR_TABLE0_9			= 0x99,
	SM5714_FG_ADDR_TABLE0_A			= 0x9A,
	SM5714_FG_ADDR_TABLE0_B			= 0x9B,
	SM5714_FG_ADDR_TABLE0_C			= 0x9C,
	SM5714_FG_ADDR_TABLE0_D			= 0x9D,
	SM5714_FG_ADDR_TABLE0_E			= 0x9E,
	SM5714_FG_ADDR_TABLE0_F			= 0x9F,
	SM5714_FG_ADDR_TABLE0_10		= 0xA0,
	SM5714_FG_ADDR_TABLE0_11		= 0xA1,
	SM5714_FG_ADDR_TABLE0_12		= 0xA2,
	SM5714_FG_ADDR_TABLE0_13		= 0xA3,
	SM5714_FG_ADDR_TABLE0_14		= 0xA4,
	SM5714_FG_ADDR_TABLE0_15		= 0xA5,
	SM5714_FG_ADDR_TABLE0_16		= 0xA6,
	SM5714_FG_ADDR_TABLE0_17		= 0xA7,
	SM5714_FG_ADDR_TABLE1_0			= 0xA8,
	SM5714_FG_ADDR_TABLE1_1			= 0xA9,
	SM5714_FG_ADDR_TABLE1_2			= 0xAA,
	SM5714_FG_ADDR_TABLE1_3			= 0xAB,
	SM5714_FG_ADDR_TABLE1_4			= 0xAC,
	SM5714_FG_ADDR_TABLE1_5			= 0xAD,
	SM5714_FG_ADDR_TABLE1_6			= 0xAE,
	SM5714_FG_ADDR_TABLE1_7			= 0xAF,
	SM5714_FG_ADDR_TABLE1_8			= 0xB0,
	SM5714_FG_ADDR_TABLE1_9			= 0xB1,
	SM5714_FG_ADDR_TABLE1_A			= 0xB2,
	SM5714_FG_ADDR_TABLE1_B			= 0xB3,
	SM5714_FG_ADDR_TABLE1_C			= 0xB4,
	SM5714_FG_ADDR_TABLE1_D			= 0xB5,
	SM5714_FG_ADDR_TABLE1_E			= 0xB6,
	SM5714_FG_ADDR_TABLE1_F			= 0xB7,
	SM5714_FG_ADDR_TABLE1_10		= 0xB8,
	SM5714_FG_ADDR_TABLE1_11		= 0xB9,
	SM5714_FG_ADDR_TABLE1_12		= 0xBA,
	SM5714_FG_ADDR_TABLE1_13		= 0xBB,
	SM5714_FG_ADDR_TABLE1_14		= 0xBC,
	SM5714_FG_ADDR_TABLE1_15		= 0xBD,
	SM5714_FG_ADDR_TABLE1_16		= 0xBE,
	SM5714_FG_ADDR_TABLE1_17		= 0xBF,
	SM5714_FG_ADDR_TABLE2_0			= 0xC0,
	SM5714_FG_ADDR_TABLE2_1			= 0xC1,
	SM5714_FG_ADDR_TABLE2_2			= 0xC2,
	SM5714_FG_ADDR_TABLE2_3			= 0xC3,
	SM5714_FG_ADDR_TABLE2_4			= 0xC4,
	SM5714_FG_ADDR_TABLE2_5			= 0xC5,
	SM5714_FG_ADDR_TABLE2_6			= 0xC6,
	SM5714_FG_ADDR_TABLE2_7			= 0xC7,
	SM5714_FG_ADDR_TABLE2_8			= 0xC8,
	SM5714_FG_ADDR_TABLE2_9			= 0xC9,
	SM5714_FG_ADDR_TABLE2_A			= 0xCA,
	SM5714_FG_ADDR_TABLE2_B			= 0xCB,
	SM5714_FG_ADDR_TABLE2_C			= 0xCC,
	SM5714_FG_ADDR_TABLE2_D			= 0xCD,
	SM5714_FG_ADDR_TABLE2_E			= 0xCE,
	SM5714_FG_ADDR_TABLE2_F			= 0xCF,

    SM5714_FG_REG_END,
};

enum sm5714_irq_source {
    MUIC_INT1 = 0,
    MUIC_INT2,
    CHG_INT1,
    CHG_INT2,
    CHG_INT3,
    CHG_INT4,
    CHG_INT5,
    FG_INT,

    SM5714_IRQ_GROUP_NR,
};
#define SM5714_NUM_IRQ_MUIC_REGS    2
#define SM5714_NUM_IRQ_CHG_REGS     5


enum sm5714_irq {
    SM5714_MUIC_IRQ_WORK = (-2),            /* -2 */
    SM5714_MUIC_IRQ_PROBE = (-1),           /* -1 */
    /* MUIC INT1 */
    SM5714_MUIC_IRQ_INT1_DPDM_OVP = 0,      /* 00 */
    SM5714_MUIC_IRQ_INT1_VBUS_RID_DETACH,   /* 01 */
    SM5714_MUIC_IRQ_INT1_RID_DETECT,        /* 02 */
    SM5714_MUIC_IRQ_INT1_CHGTYPE,           /* 03 */
    SM5714_MUIC_IRQ_INT1_DCDTIMEOUT,        /* 04 */
    /* MUIC INT2 */
    SM5714_MUIC_IRQ_INT2_AFC_ERROR,         /* 05 */
    SM5714_MUIC_IRQ_INT2_AFC_STA_CHG,       /* 06 */
    SM5714_MUIC_IRQ_INT2_MULTI_BYTE,        /* 07 */
    SM5714_MUIC_IRQ_INT2_VBUS_UPDATE,       /* 08 */
    SM5714_MUIC_IRQ_INT2_AFC_ACCEPTED,      /* 09 */
    SM5714_MUIC_IRQ_INT2_AFC_TA_ATTACHED,   /* 10 */
    /* CHG INT1 */
    SM5714_CHG_IRQ_INT1_VBUSLIMIT,          /* 11 */
    SM5714_CHG_IRQ_INT1_VBUSOVP,            /* 12 */
    SM5714_CHG_IRQ_INT1_VBUSUVLO,           /* 13 */
    SM5714_CHG_IRQ_INT1_VBUSPOK,            /* 14 */
    /* CHG INT2 */
    SM5714_CHG_IRQ_INT2_WDTMROFF,           /* 15 */
    SM5714_CHG_IRQ_INT2_DONE,               /* 16 */
    SM5714_CHG_IRQ_INT2_TOPOFF,             /* 17 */
    SM5714_CHG_IRQ_INT2_Q4FULLON,           /* 18 */
    SM5714_CHG_IRQ_INT2_CHGON,              /* 19 */
    SM5714_CHG_IRQ_INT2_NOBAT,              /* 20 */
    SM5714_CHG_IRQ_INT2_BATOVP,             /* 21 */
    SM5714_CHG_IRQ_INT2_AICL,               /* 22 */
    /* CHG INT3 */
    SM5714_CHG_IRQ_INT3_VSYSOVP,            /* 23 */
    SM5714_CHG_IRQ_INT3_nENQ4,              /* 24 */
    SM5714_CHG_IRQ_INT3_FASTTMROFF,         /* 25 */
    SM5714_CHG_IRQ_INT3_TRICKLETMROFF,      /* 26 */
    SM5714_CHG_IRQ_INT3_DISLIMIT,           /* 27 */
    SM5714_CHG_IRQ_INT3_OTGFAIL,            /* 28 */
    SM5714_CHG_IRQ_INT3_THEMSHDN,           /* 29 */
    SM5714_CHG_IRQ_INT3_THEMREG,            /* 30 */
    /* CHG INT4 */
    SM5714_CHG_IRQ_INT4_CVMODE,             /* 31 */
    SM5714_CHG_IRQ_INT4_BOOSTPOK,           /* 32 */
    SM5714_CHG_IRQ_INT4_BOOSTPOK_NG,        /* 33 */
    /* CHG INT5 */
    SM5714_CHG_IRQ_INT5_ABSTMROFF,          /* 34 */
    SM5714_CHG_IRQ_INT5_FLEDOPEN,           /* 35 */
    SM5714_CHG_IRQ_INT5_FLEDSHORT,          /* 36 */
    /* FG INT */
    SM5714_FG_IRQ_INT_LOW_VOLTAGE,          /* 37 */

    SM5714_IRQ_NR,
};

struct sm5714_dev {
	struct device *dev;
	struct i2c_client *charger;     /* 0x92; Charger */
	struct i2c_client *fuelgauge;   /* 0xE2; Fuelgauge */
	struct i2c_client *muic;        /* 0x4A; MUIC */
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
    void (*check_muic_reset)(struct i2c_client *, void *);
    void (*check_chg_reset)(struct i2c_client *, void *);
    void (*check_fg_reset)(struct i2c_client *, void *);
    void *muic_data;
    void *chg_data;
    void *fg_data;

	u8 pmic_rev;
	u8 vender_id;

	struct sm5714_platform_data *pdata;
};

enum sm5714_types {
	TYPE_SM5714,
};

enum sm5714_dev_types {
	DEV_TYPE_SM5714_MUIC = 0x0,
	DEV_TYPE_SM5714_PDIC = 0x1,
};

extern int sm5714_irq_init(struct sm5714_dev *sm5714);
extern void sm5714_irq_exit(struct sm5714_dev *sm5714);

/* SM5714 shared i2c API function */
extern int sm5714_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int sm5714_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
extern int sm5714_read_word(struct i2c_client *i2c, u8 reg);
extern int sm5714_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int sm5714_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
extern int sm5714_write_word(struct i2c_client *i2c, u8 reg, u16 value);

extern int sm5714_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sm5714_update_word(struct i2c_client *i2c, u8 reg, u16 val, u16 mask);

/* support SM5714 Charger operation mode control module */
enum {
    SM5714_CHARGER_OP_EVENT_VBUSIN      = 0x5,
	SM5714_CHARGER_OP_EVENT_USB_OTG     = 0x4,
    SM5714_CHARGER_OP_EVENT_PWR_SHAR    = 0x3,
    SM5714_CHARGER_OP_EVENT_FLASH       = 0x2,
    SM5714_CHARGER_OP_EVENT_TORCH       = 0x1,
    SM5714_CHARGER_OP_EVENT_SUSPEND     = 0x0,
};

extern int sm5714_charger_oper_table_init(struct sm5714_dev *sm5714);
extern int sm5714_charger_oper_push_event(int event_type, bool enable);
extern int sm5714_charger_oper_get_current_status(void);
extern int sm5714_charger_oper_get_current_op_mode(void);
extern int sm5714_charger_oper_en_factory_mode(int dev_type, int rid, bool enable);
extern int sm5714_charger_oper_forced_vbus_limit_control(int mA);
#endif /* __SM5714_PRIV_H__ */

