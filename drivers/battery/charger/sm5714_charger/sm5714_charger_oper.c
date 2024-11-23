/*
 * sm5714-charger.c - SM5714 Charger operation mode control module.
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/delay.h>

#if IS_ENABLED(CONFIG_MFD_SM5714)
#include <linux/mfd/sm/sm5714/sm5714.h>
#include <linux/mfd/sm/sm5714/sm5714-private.h>
#else
#include "sm5714_fake_mfd_chg.h"
#endif
#include <linux/usb/typec/common/pdic_notifier.h>
#include "../../common/sec_charging_common.h"

enum {
	OP_MODE_SUSPEND     = 0x0,
	OP_MODE_CHG_ON_VBUS = 0x5,
	OP_MODE_USB_OTG     = 0x7,
	OP_MODE_FLASH_BOOST = 0x8,
};

enum {
	BSTOUT_4400mV   = 0x0,
	BSTOUT_4500mV   = 0x1,
	BSTOUT_4600mV   = 0x2,
	BSTOUT_4700mV   = 0x3,
	BSTOUT_4800mV   = 0x4,
	BSTOUT_5000mV   = 0x5,
	BSTOUT_5100mV   = 0x6,
	BSTOUT_5200mV   = 0x7,

	BSTOUT_5300mV   = 0x9,
	BSTOUT_5400mV   = 0xA,
	BSTOUT_5500mV   = 0xB,
};

enum {
	OTG_CURRENT_500mA   = 0x0,
	OTG_CURRENT_900mA   = 0x1,
	OTG_CURRENT_1200mA  = 0x2,
	OTG_CURRENT_1500mA  = 0x3,
};

#define make_OP_STATUS(vbus, otg, pwr_shar, flash, torch, suspend)  \
			(((vbus & 0x1)      << SM5714_CHARGER_OP_EVENT_VBUSIN)     | \
			((otg & 0x1)       << SM5714_CHARGER_OP_EVENT_USB_OTG)     | \
			((pwr_shar & 0x1)  << SM5714_CHARGER_OP_EVENT_PWR_SHAR)    | \
			((flash & 0x1)     << SM5714_CHARGER_OP_EVENT_FLASH)       | \
			((torch & 0x1)     << SM5714_CHARGER_OP_EVENT_TORCH)       | \
			((suspend & 0x1)   << SM5714_CHARGER_OP_EVENT_SUSPEND))

struct sm5714_charger_oper_table_info {
	unsigned short status;
	unsigned char oper_mode;
	unsigned char BST_OUT;
	unsigned char OTG_CURRENT;
};

struct sm5714_charger_oper_info {
	struct i2c_client *i2c;
	struct mutex op_mutex;
	int max_table_num;
	int table_index;
	struct sm5714_charger_oper_table_info current_table;
	/* for Factory mode control */
	unsigned char factory_RID;
	int chg_float_voltage;
	bool set_factory_619k;
	bool set_fledon_buckoff_state;
};
static struct sm5714_charger_oper_info *oper_info;

/**
 *  (VBUS in/out) (USB-OTG in/out) (PWR-SHAR in/out)
 *  (Flash on/off) (Torch on/off) (Suspend mode on/off)
 **/
static struct sm5714_charger_oper_table_info sm5714_charger_op_mode_table[] = {
	/* Charger=ON Mode in a valid Input */
	{ make_OP_STATUS(0, 0, 0, 0, 0, 0),
					OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 0, 0, 0, 0, 0),
					OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 1, 0, 0, 0, 0), /* Prevent : VBUS + OTG timing sync */
					OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 0, 0, 0, 1, 0),
					OP_MODE_CHG_ON_VBUS, BSTOUT_4500mV, OTG_CURRENT_500mA},
	/* Flash Boost Mode */
	{ make_OP_STATUS(0, 0, 0, 1, 0, 0),
					OP_MODE_FLASH_BOOST, BSTOUT_5100mV, OTG_CURRENT_500mA},
	{ make_OP_STATUS(1, 0, 0, 1, 0, 0),
					OP_MODE_FLASH_BOOST, BSTOUT_5100mV, OTG_CURRENT_500mA},

	{ make_OP_STATUS(0, 0, 1, 1, 0, 0),
					OP_MODE_FLASH_BOOST, BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 0, 1, 1, 0),
					OP_MODE_FLASH_BOOST, BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 0, 0, 1, 0),
					OP_MODE_FLASH_BOOST, BSTOUT_5100mV, OTG_CURRENT_900mA},
	/* USB OTG Mode */
	{ make_OP_STATUS(0, 1, 0, 0, 0, 0),
					OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 1, 0, 0, 0),
					OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 1, 0, 1, 0, 0),
					OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 1, 0, 0, 1, 0),
					OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	{ make_OP_STATUS(0, 0, 1, 0, 1, 0),
					OP_MODE_USB_OTG,     BSTOUT_5100mV, OTG_CURRENT_900mA},
	/* Suspend Mode : Reserved position of SUSPEND mode table */
	{ make_OP_STATUS(0, 0, 0, 0, 0, 1),
					OP_MODE_SUSPEND,     BSTOUT_5100mV, OTG_CURRENT_900mA},
};

static void sm5714_charger_oper_set_batreg(u16 float_voltage)
{
	u8 offset;


	if (float_voltage <= 3700)
		offset = 0x0;
	else if (float_voltage < 3900)
		offset = ((float_voltage - 3700) / 50);    /* BATREG = 3.70 ~ 3.85V in 0.05V steps */
	else if (float_voltage < 4050)
		offset = (((float_voltage - 3900) / 100) + 4);    /* BATREG = 3.90, 4.0V in 0.1V steps */
	else if (float_voltage < 4630)
		offset = (((float_voltage - 4050) / 10) + 6);    /* BATREG = 4.05 ~ 4.62V in 0.01V steps */
	else {
		pr_err("sm5714-charger: %s: can't support BATREG at over voltage 4.62V (mV=%d)\n",
			__func__, float_voltage);
		offset = 0x15;    /* default Offset : 4.2V */
	}

	pr_info("%s:  set as  (mV=%d) batreg Control\n", __func__, float_voltage);

	sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CHGCNTL4, ((offset & 0x3F) << 0), (0x3F << 0));
}

static int set_OP_MODE(struct i2c_client *i2c, u8 mode)
{
	return sm5714_update_reg(i2c, SM5714_CHG_REG_CNTL2, (mode << 0), (0xF << 0));
}

static int set_BSTOUT(struct i2c_client *i2c, u8 bstout)
{
	return sm5714_update_reg(i2c, SM5714_CHG_REG_BSTCNTL1, (bstout << 0), (0xF << 0));
}

static int set_OTG_CURRENT(struct i2c_client *i2c, u8 otg_curr)
{
	return sm5714_update_reg(i2c, SM5714_CHG_REG_BSTCNTL1, (otg_curr << 6), (0x3 << 6));
}

static inline int change_op_table(unsigned char new_status)
{
	int i = 0;

	pr_info("%s: Old table[%d] info (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x)\n",
			__func__, oper_info->table_index, oper_info->current_table.status,
			oper_info->current_table.oper_mode, oper_info->current_table.BST_OUT,
			oper_info->current_table.OTG_CURRENT);

	/* Check actvated Suspend Mode */
	if (new_status & (0x1 << SM5714_CHARGER_OP_EVENT_SUSPEND)) {
		i = oper_info->max_table_num - 1;    /* Reserved SUSPEND Mode Table index */
	} else {
		/* Search matched Table */
		for (i = 0; i < oper_info->max_table_num; ++i) {
			if (new_status == sm5714_charger_op_mode_table[i].status)
				break;
		}
	}
	if (i == oper_info->max_table_num) {
		pr_err("%s: can't find matched charger op_mode table (status = 0x%x)\n", __func__, new_status);
		return -EINVAL;
	}

    /* Update current table info */

	set_BSTOUT(oper_info->i2c, sm5714_charger_op_mode_table[i].BST_OUT);
	oper_info->current_table.BST_OUT = sm5714_charger_op_mode_table[i].BST_OUT;

	set_OTG_CURRENT(oper_info->i2c, sm5714_charger_op_mode_table[i].OTG_CURRENT);
	oper_info->current_table.OTG_CURRENT = sm5714_charger_op_mode_table[i].OTG_CURRENT;

	/* Factory 523K-JIG Test : Torch Light - Prevent VBUS input source */
	if ((sm5714_charger_op_mode_table[i].status & 0x02) &&
			(oper_info->factory_RID == RID_255K || oper_info->factory_RID == RID_523K)) {
		pr_info("sm5714-charger: %s: skip Flash Boost mode for Factory JIG fled:torch test\n", __func__);
	/* Factory 523K-JIG Test : Flash Light - Prevent VBUS input source */
	} else if ((sm5714_charger_op_mode_table[i].status & 0x04) &&
			(oper_info->factory_RID == RID_255K || oper_info->factory_RID == RID_523K)) {
		pr_info("sm5714-charger: %s: skip Flash Boost mode for Factory JIG fled:flash test\n", __func__);
	} else {
		/* W/A : 
		 * When operating a torch or flash in buck-off state, change to flash_boost mode instead of suspend mode. 
		 */
		if (oper_info->set_fledon_buckoff_state) {
			if (i == oper_info->max_table_num - 1) {
				if ( (new_status & (0x1 << SM5714_CHARGER_OP_EVENT_TORCH)) || 
					 (new_status & (0x1 << SM5714_CHARGER_OP_EVENT_FLASH)) ) {
					pr_info("sm5714-charger: %s: Set Flash Boost mode in buck-off status\n", __func__);
					sm5714_charger_op_mode_table[i].oper_mode = OP_MODE_FLASH_BOOST;
				} else {
					sm5714_charger_op_mode_table[i].oper_mode = OP_MODE_SUSPEND;
				}
			}
		}
		set_OP_MODE(oper_info->i2c, sm5714_charger_op_mode_table[i].oper_mode);
		oper_info->current_table.oper_mode = sm5714_charger_op_mode_table[i].oper_mode;
	}
	oper_info->current_table.status = new_status;
	oper_info->table_index = i;

	pr_info("%s: New table[%d] (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x)\n",
			__func__, oper_info->table_index, oper_info->current_table.status,
			oper_info->current_table.oper_mode, oper_info->current_table.BST_OUT,
			oper_info->current_table.OTG_CURRENT);

	return 0;
}

static inline unsigned char update_status(int event_type, bool enable)
{
	if (event_type > SM5714_CHARGER_OP_EVENT_VBUSIN) {
		pr_debug("sm5714-charger: %s: invalid event type (type=0x%x)\n", __func__, event_type);
		return oper_info->current_table.status;
	}

	if (enable)
		return (oper_info->current_table.status | (1 << event_type));
	else
		return (oper_info->current_table.status & ~(1 << event_type));
}

int sm5714_charger_oper_push_event(int event_type, bool enable)
{
	unsigned char new_status;
	int ret = 0;

	if (oper_info == NULL) {
		pr_err("sm5714-charger: %s: required init op_mode table\n", __func__);
		return -ENOENT;
	}
	pr_info("sm5714-charger: %s: event_type=%d, enable=%d\n", __func__, event_type, enable);

	mutex_lock(&oper_info->op_mutex);

	new_status = update_status(event_type, enable);
	if (new_status == oper_info->current_table.status)
		goto out;

	ret = change_op_table(new_status);

out:
	mutex_unlock(&oper_info->op_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_push_event);

static inline int detect_initial_table_index(struct i2c_client *i2c)
{
	return 0;
}
int sm5714_charger_oper_table_init(struct sm5714_dev *sm5714)
{
	struct i2c_client *i2c = sm5714->charger;
	int index, ret = 0;
	struct device_node *np = NULL;

	if (oper_info) {
		pr_info("sm5714-charger: %s: already initialized\n", __func__);
		return 0;
	}

	if (i2c == NULL) {
		pr_err("sm5714-charger: %s: invalid i2c client handler=n", __func__);
		return -EINVAL;
	}

	oper_info = kmalloc(sizeof(struct sm5714_charger_oper_info), GFP_KERNEL);
	if (oper_info == NULL)
		return -ENOMEM;

	oper_info->i2c = i2c;

	mutex_init(&oper_info->op_mutex);

	/* set default operation mode condition */
	oper_info->max_table_num = ARRAY_SIZE(sm5714_charger_op_mode_table);
	index = detect_initial_table_index(oper_info->i2c);
	oper_info->table_index = index;
	oper_info->current_table.status = sm5714_charger_op_mode_table[index].status;
	oper_info->current_table.oper_mode = sm5714_charger_op_mode_table[index].oper_mode;
	oper_info->current_table.BST_OUT = sm5714_charger_op_mode_table[index].BST_OUT;
	oper_info->current_table.OTG_CURRENT = sm5714_charger_op_mode_table[index].OTG_CURRENT;

	set_OP_MODE(oper_info->i2c, oper_info->current_table.oper_mode);
	set_BSTOUT(oper_info->i2c, oper_info->current_table.BST_OUT);
	set_OTG_CURRENT(oper_info->i2c, oper_info->current_table.OTG_CURRENT);

	oper_info->factory_RID = 0;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		dev_err(sm5714->dev, "%s: can't find battery node\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage", &oper_info->chg_float_voltage);
		if (ret) {
			dev_info(sm5714->dev, "%s: battery,chg_float_voltage is Empty\n", __func__);
			oper_info->chg_float_voltage = 4350;
		}
		pr_info("%s: battery,chg_float_voltage is %d\n", __func__, oper_info->chg_float_voltage);

		oper_info->set_factory_619k = of_property_read_bool(np, "battery,set_factory_619k");
		pr_info("%s: battery,set_factory_619k %d\n", __func__,
			oper_info->set_factory_619k);
		oper_info->set_fledon_buckoff_state = of_property_read_bool(np, "battery,set_fledon_buckoff_state");
		pr_info("%s: battery,set_fledon_buckoff_state %d\n", __func__,
			oper_info->set_fledon_buckoff_state);
	}

	pr_info("%s: current table[%d] (STATUS: 0x%x, MODE: %d, BST_OUT: 0x%x, OTG_CURRENT: 0x%x)\n",
			__func__, oper_info->table_index, oper_info->current_table.status,
			oper_info->current_table.oper_mode, oper_info->current_table.BST_OUT,
			oper_info->current_table.OTG_CURRENT);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_table_init);

int sm5714_charger_oper_get_current_status(void)
{
	if (oper_info == NULL)
		return -EINVAL;

	return oper_info->current_table.status;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_get_current_status);

int sm5714_charger_oper_get_current_op_mode(void)
{
	if (oper_info == NULL)
		return -EINVAL;

	return oper_info->current_table.oper_mode;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_get_current_op_mode);

int sm5714_charger_oper_en_factory_mode(int dev_type, int rid, bool enable)
{
	u8 reg = 0x0;
	union power_supply_propval val = {0, };

	if (oper_info == NULL)
		return -EINVAL;

	if (enable) {
		switch (rid) {
		case RID_523K:
			if (oper_info->set_factory_619k) {
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL2,
					0x00, 0x0F);	/* SUSPEND MODE */
			}
			sm5714_charger_oper_set_batreg(4200);
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1,
				(0x0 << 6), (0x1 << 6));	/* AICLEN_VBUS = 0 (Disable) */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1,
				(0x1 << 0), (0x1 << 0));	/* NOZX = 1 (Disable) */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL,
				(0x7F << 0), (0x7F << 0));	/* VBUS_LIMIT = MAX(3275mA) */
			break;
		case RID_301K:
			if (oper_info->set_factory_619k) {
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL2,
					0x05, 0x0F);	/* CHG_ON MODE */
			}
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1,
				(0x0 << 6), (0x1 << 6));	/* AICLEN_VBUS = 0 (Disable) */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1,
				(0x0 << 0), (0x1 << 0));	/* NOZX = 0 (Enable) */
#if defined(CONFIG_SEC_FACTORY)
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL,
				(0x7F << 0), (0x7F << 0));	/* VBUS_LIMIT = MAX(3275mA) */
#else
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL,
				(0x44 << 0), (0x7F << 0));	/* VBUS_LIMIT = MAX(1800mA) */
#endif
			break;
		case RID_619K:
			if (oper_info->set_factory_619k) {
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL2,
					0x05, 0x0F);	/* CHG_ON MODE */
				sm5714_charger_oper_set_batreg(4200);
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1,
					(0x0 << 6), (0x1 << 6));	/* AICLEN_VBUS = 0 (Disable) */
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1,
					(0x1 << 0), (0x1 << 0));	/* NOZX = 1 (Disable) */
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL,
					(0x7F << 0), (0x7F << 0));	/* VBUS_LIMIT = MAX(3275mA) */
			} else {
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1,
					(0x0 << 6), (0x1 << 6));	/* AICLEN_VBUS = 0 (Disable) */
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1,
					(0x0 << 0), (0x1 << 0));	/* NOZX = 0 (Enable) */
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL,
					(0x44 << 0), (0x7F << 0));	/* VBUS_LIMIT = 1800mA */
			}
			break;
		case RID_255K:
			if (oper_info->set_factory_619k) {
				sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL2,
					0x00, 0x0F);	/* SUSPEND MODE */
			}
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1,
				(0x0 << 6), (0x1 << 6));	/* AICLEN_VBUS = 0 (Disable) */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1,
				(0x0 << 0), (0x1 << 0));	/* NOZX = 0 (Enable) */
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL,
				(0x7F << 0), (0x7F << 0));	/* VBUS_LIMIT = MAX(3275mA) */
			break;
		}

		psy_do_property("sm5714-fuelgauge", set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);

		oper_info->factory_RID = rid;

		sm5714_read_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, &reg);
		pr_info("%s: enable factory mode configuration(RID=%d, vbuslimit=0x%02X)\n", __func__, rid, reg);
	} else {
		if (oper_info->set_factory_619k) {
			sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL2,
				0x05, 0x0F);	/* CHG_ON MODE */
		}
		sm5714_charger_oper_set_batreg(oper_info->chg_float_voltage);

		sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CHGCNTL11,
			(0x0 << 0), (0x1 << 0));		/* forced_vsys	= disable */

		sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_CNTL1,
			(0x1 << 6), (0x1 << 6));		/* AICLEN_VBUS	= 1 (Enable) */
		sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_FACTORY1,
			(0x0 << 0), (0x1 << 0));		/* NOZX			= 0 (Enable) */
		sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL,
			(0x10 << 0), (0x7F << 0));		/* VBUS_LIMIT	= 500mA */

		oper_info->factory_RID = 0;
		sm5714_read_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, &reg);
		pr_info("%s: disable factory mode configuration(vbuslimit=0x%02X)\n", __func__, reg);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_en_factory_mode);

int sm5714_charger_oper_forced_vbus_limit_control(int mA)
{

	u8 old_offset, new_offset;
	int old_mA;
	int msec;

	sm5714_read_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, &old_offset);
	old_mA = ((old_offset & 0x7F) * 25) + 100;

	new_offset = ((mA - 100) / 25);
	sm5714_update_reg(oper_info->i2c, SM5714_CHG_REG_VBUSCNTL, ((new_offset & 0x7F) << 0), (0x7F << 0));

	msec = (old_mA - mA) / 10;		// 10mA/ms
	if (msec < 0)
		msec *= (-1);

	msleep(msec);

	pr_info("sm5714-charger: %s VBUSLIMIT control 0x%X[%dmA] -> 0x%X[%dmA] (%d ms)\n",
		__func__, old_offset, old_mA, new_offset, mA, msec);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_charger_oper_forced_vbus_limit_control);
