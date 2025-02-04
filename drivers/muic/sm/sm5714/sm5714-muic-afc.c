/*
 * sm5714-muic-afc.c - afc driver for the SiliconMitus sm5714
 *
 *  Copyright (C) 2017 SiliconMitus
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * This driver is based on max77843-muic-afc.c
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/device.h>
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "../../../battery/common/sec_battery.h"
#endif

#include <linux/mfd/sm/sm5714/sm5714.h>
#include <linux/mfd/sm/sm5714/sm5714-private.h>

/* MUIC header file */
#include <linux/muic/common/muic.h>
#include <linux/muic/sm/sm5714/sm5714-muic.h>


#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

static struct sm5714_muic_data *afc_init_data;

static void sm5714_afc_notifier_attach(struct sm5714_muic_data *muic_data,
	int afcta, int txt_voltage);

static int sm5714_muic_get_vbus(void)
{
	struct sm5714_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	int vbus_voltage = 0, voltage = 0;
	int irqvbus = 0, intmask2 = 0;
	int retry = 0;
	int vbus_valid = 0, reg_vbus = 0;

	reg_vbus = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS);
	vbus_valid = (reg_vbus&0x04)>>2;
	pr_info("[%s:%s] REG_VBUS:0x%x, vbus_valid(%d)", MUIC_DEV_NAME,
		__func__, reg_vbus, vbus_valid);
	if (!vbus_valid) {
		pr_info("[%s:%s] skip : NO VBUS\n", MUIC_DEV_NAME, __func__);
		return 0;
	}

	intmask2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_INTMASK2);
	pr_info("[%s:%s] REG_INTMASK2:0x%x\n", MUIC_DEV_NAME,
		__func__, intmask2);
	if (!(intmask2&INT2_VBUS_UPDATE_MASK)) {
		intmask2 = intmask2 | INT2_VBUS_UPDATE_MASK;
		sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_INTMASK2, intmask2);
	}

	sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_VBUS_READ, 1);

	for (retry = 0; retry < 5 ; retry++) {
		usleep_range(5000, 5100);
		irqvbus = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_INT2);
		if (irqvbus & INT2_VBUS_UPDATE_MASK) {
			pr_info("[%s:%s] VBUS update Success(%d), retry: (%d)\n",
				MUIC_DEV_NAME, __func__, irqvbus, retry);
			break;
		}
		pr_info("[%s:%s] VBUS update Fail(%d), retry: (%d)\n",
				MUIC_DEV_NAME, __func__, irqvbus, retry);
	}

	sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_VBUS_READ, 0);

	if (retry >= 5) {
		pr_info("[%s:%s] VBUS update Failed(%d)\n", MUIC_DEV_NAME,
				__func__, retry);
		return 0;
	}

	voltage = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS_VOLTAGE);
	if (voltage < 0)
		pr_err("[%s:%s] err read VBUS VOLTAGE(0x%2x)\n", MUIC_DEV_NAME,
				__func__, voltage);

	vbus_voltage = voltage*100;

	pr_info("[%s:%s] voltage=[0x%02x] vbus_voltage=%d mV, attached_dev(%d)\n",
		MUIC_DEV_NAME, __func__, voltage, vbus_voltage,
		muic_data->attached_dev);

	return vbus_voltage;
}

int sm5714_muic_get_vbus_voltage(void)
{
	struct sm5714_muic_data *muic_data = afc_init_data;
	int vbus_voltage = 0;

	mutex_lock(&muic_data->afc_mutex);
	vbus_voltage = sm5714_muic_get_vbus();
	mutex_unlock(&muic_data->afc_mutex);

	pr_info("[%s:%s] vbus_voltage=%d mV\n",
		MUIC_DEV_NAME, __func__, vbus_voltage);

	return vbus_voltage;
}
EXPORT_SYMBOL(sm5714_muic_get_vbus_voltage);

int muic_request_disable_afc_state(void)
{
	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	muic_disable_afc(1); /* 9V(12V) -> 5V */

	return 0;
}
EXPORT_SYMBOL(muic_request_disable_afc_state);

int muic_check_fled_state(int enable, int mode)
{
	struct sm5714_muic_data *muic_data = afc_init_data;

	pr_info("[%s:%s] enable(%d), mode(%d)\n", MUIC_DEV_NAME, __func__,
			enable, mode);

	if (mode == FLED_MODE_TORCH) { /* torch */
		cancel_delayed_work(&muic_data->afc_torch_work);
		pr_info("[%s:%s] afc_torch_work cancel\n",
				MUIC_DEV_NAME, __func__);

		muic_data->fled_torch_enable = enable;
	} else if (mode == FLED_MODE_FLASH) { /* flash */
		muic_data->fled_flash_enable = enable;
		if (enable) {
			cancel_delayed_work(&muic_data->afc_torch_work);
			pr_info("[%s:%s] afc_torch_work cancel\n",
				MUIC_DEV_NAME, __func__);
		}
	}

	pr_info("[%s:%s] fled_torch_enable(%d), fled_flash_enable(%d)\n",
			MUIC_DEV_NAME, __func__, muic_data->fled_torch_enable,
			muic_data->fled_flash_enable);

	if ((muic_data->fled_torch_enable == false) &&
			(muic_data->fled_flash_enable == false)) {
		if (muic_data->hv_voltage == 5) {
			pr_info("[%s:%s] skip high voltage setting\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}
		if ((mode == FLED_MODE_TORCH) && (enable == false)) {
			cancel_delayed_work(&muic_data->afc_torch_work);
			schedule_delayed_work(&muic_data->afc_torch_work,
					msecs_to_jiffies(5000));
			pr_info("[%s:%s] afc_torch_work start(5sec)\n",
					MUIC_DEV_NAME, __func__);
		} else {
			muic_disable_afc(0);  /* 5V -> 9V(12V) */
		}
	}

	return 0;
}
EXPORT_SYMBOL(muic_check_fled_state);

bool sm5714_is_afc_disabled(struct sm5714_muic_data *muic_data)
{
	bool ret = (muic_data->fled_torch_enable == 1) ||
				(muic_data->fled_flash_enable == 1) ||
				(muic_data->is_water_detect);

	if (ret) {
		pr_info("[%s:%s] Torch(%d), Flash(%d), Water(%d), AFC DISABLED\n",
				MUIC_DEV_NAME, __func__,
				muic_data->fled_torch_enable,
				muic_data->fled_flash_enable,
				muic_data->is_water_detect);
	}
	return ret;
}

int muic_disable_afc(int disable)
{
	struct sm5714_muic_data *muic_data = afc_init_data;
	int ret = 0;

	pr_info("[%s:%s] disable: %d\n", MUIC_DEV_NAME, __func__, disable);

	if (disable) { /* AFC disable : 9V(12V) -> 5V */
		switch (muic_data->attached_dev) {
		case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
			pr_info("[%s:%s] AFC TA attached_dev(%d)\n",
				MUIC_DEV_NAME, __func__,
				muic_data->attached_dev);

			sm5714_muic_voltage_control(muic_data,
				SM5714_MUIC_HV_5V, SM5714_MUIC_AFC_TA);

			break;
		case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
			pr_info("[%s:%s] QC20 TA attached_dev(%d)\n",
				MUIC_DEV_NAME, __func__,
				muic_data->attached_dev);

			sm5714_muic_voltage_control(muic_data, SM5714_ENQC20_5V,
					SM5714_MUIC_QC20);
			break;
		default:
			pr_info("[%s:%s] skip: attached_dev(%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
			break;
		}
	} else { /* AFC enable : 5V -> 9V(12V) */
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
		if (muic_data->pdic_afc_state == SM5714_MUIC_AFC_ABNORMAL) {
			pr_info("[%s:%s] pdic abnormal: AFC(QC20) skip\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}
#endif

		switch (muic_data->attached_dev) {
		case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
		case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
			pr_info("[%s:%s] attached_dev(%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
			sm5714_muic_voltage_control(muic_data,
					SM5714_MUIC_HV_9V,
					SM5714_MUIC_AFC_TA);

			break;
		case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
			pr_info("[%s:%s] attached_dev(%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
			sm5714_muic_voltage_control(muic_data, SM5714_ENQC20_9V,
					SM5714_MUIC_QC20);
			break;
		default:
			pr_info("[%s:%s] skip: attached_dev(%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
			break;
		}
	}

	return ret;
}

int sm5714_muic_voltage_control(struct sm5714_muic_data *muic_data,
	int afctxd, int qc20)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0, reg_val = 0;
	int retry = 0;
	int intmask2 = 0;
	int irqafc = 0;
	int afcstatus = 0;
	int afc_error_count = 0;
	int dev1 = 0;
	int attached_dev = 0;
#if !defined(CONFIG_MUIC_QC_DISABLE)
	int qc20_temp = 0;
#endif
	int vbus_voltage = 0;
	int vbus_txd_voltage = 0;
	int voltage_min = 0, voltage_max = 0;
	int i = 0, vbus_check = 0;
	union power_supply_propval value;

	pr_info("[%s:%s] afctxd(0x%x), qc20(0x%x)\n",
			MUIC_DEV_NAME, __func__, afctxd, qc20);

	mutex_lock(&muic_data->afc_mutex);
	/* QC20 */
	if (qc20 == SM5714_MUIC_QC20) {
		pr_info("[%s:%s]QC20: afctxd(0x%x)\n",
			MUIC_DEV_NAME, __func__, afctxd);

		ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCCNTL);
		reg_val = (ret & 0x3F) | (afctxd<<6);
		sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCCNTL, reg_val);
		pr_info("[%s:%s] read REG_AFCCNTL=0x%x ,  write REG_AFCCNTL=0x%x , qc20_vbus=%d\n",
				MUIC_DEV_NAME, __func__, ret, reg_val, afctxd);

		/* VBUS check */
		if (afctxd == SM5714_ENQC20_5V)
			vbus_txd_voltage = 5;
		else if (afctxd == SM5714_ENQC20_9V)
			vbus_txd_voltage = 9;

		voltage_min = vbus_txd_voltage - 2; /* - 2V */
		voltage_max = vbus_txd_voltage + 1; /* + 1V */

		vbus_check = 0;
		for (i = 0 ; i < 5 ; i++) {
			vbus_voltage = sm5714_muic_get_vbus_value(muic_data);
			pr_info("[%s:%s]i:%d TXD:%dV voltage_min:%dV voltage_max:%dV vbus_voltage:%dV\n",
					MUIC_DEV_NAME, __func__,
					i, vbus_txd_voltage,
					voltage_min, voltage_max,
					vbus_voltage);

			if ((voltage_min <= vbus_voltage) &&
				(vbus_voltage <= voltage_max)) {
				vbus_check = 1;
				i = 10; /* break */
			} else {
				msleep(100);
				pr_info("[%s:%s] retry:%d\n",
					MUIC_DEV_NAME, __func__, i);
			}
		}
		if (vbus_check)
			sm5714_afc_notifier_attach(muic_data,
				SM5714_MUIC_QC20, vbus_txd_voltage);
		else
			sm5714_afc_notifier_attach(muic_data,
				SM5714_MUIC_QC20, 5);
	} else { /* AFC TA */
		pr_info("[%s:%s] AFC: afctxd(0x%x)\n",
				MUIC_DEV_NAME, __func__, afctxd);

		muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		
		/* Wait until less than 500mA.*/
		for (retry = 0; retry < 20; retry++) {
			usleep_range(5000, 5100);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			psy_do_property("sm5714-charger", get,
					POWER_SUPPLY_PROP_CURRENT_MAX, value);
#endif
			if (value.intval <= 500) {
				pr_info("[%s:%s]PREPARE Success(%d mA), retry(%d)\n",
						MUIC_DEV_NAME, __func__, value.intval, retry);
				break;
			}
			pr_info("[%s:%s]PREPARE fail(%d mA), retry(%d)\n", MUIC_DEV_NAME, __func__,
					value.intval, retry);
		}

		sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCTXD, afctxd);

		/* AFC(INT2) mask */
		intmask2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_INTMASK2);
		intmask2 = intmask2 | 0x3F;
		sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_INTMASK2, intmask2);

		/* ENAFC set '1' */
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);

		afc_error_count = 0;
		attached_dev = 0;

		for (retry = 0; retry < 10; retry++) {
			msleep(50);
			irqafc = sm5714_i2c_read_byte(i2c,
					SM5714_MUIC_REG_INT2);
			if (irqafc & INT2_AFC_ACCEPTED_MASK) {
				pr_info("[%s:%s] AFC_ACCEPTED Success(0x%x), retry(%d)\n",
						MUIC_DEV_NAME, __func__,
						irqafc, retry);
				break;
			}

			if (irqafc & INT2_AFC_ERROR_MASK) {
				/* read AFC_STATUS */
				afcstatus = sm5714_i2c_read_byte(i2c,
						SM5714_MUIC_REG_AFCSTATUS);
				dev1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE1);
				
				pr_info("[%s:%s] AFC_ERROR, afcstatus(0x%x), DEVICE_TYPE1(0x%x), irqafc(0x%x), retry(%d)\n",
						MUIC_DEV_NAME, __func__,
						afcstatus, dev1, irqafc, retry);
#if defined(CONFIG_MUIC_QC_DISABLE)
				if (afc_error_count >= 2) {
					break;
#else
				if ( (afc_error_count >= 2) && (dev1 & DEV_TYPE1_QC20_TA) ){
					/* ENAFC set '0' */
					sm5714_set_afc_ctrl_reg(muic_data,
							AFCCTRL_ENAFC, 0);
					pr_info("[%s:%s] QC20_TA, retry(%d)\n",
							MUIC_DEV_NAME, __func__, retry);

					attached_dev = SM5714_MUIC_QC20;

					qc20_temp = ((afctxd&0xF0)>>4);
					if (qc20_temp == 0x00) /* QC20 5V */
						qc20_temp = SM5714_ENQC20_5V;
					else if (qc20_temp == 0x04) /* QC20 9V */
						qc20_temp = SM5714_ENQC20_9V;

					ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCCNTL);
					reg_val = (ret & 0x3F) | (qc20_temp<<6);
					sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCCNTL, reg_val);
					pr_info("[%s:%s] read REG_AFCCNTL=0x%x ,  write REG_AFCCNTL=0x%x , qc20_vbus=%d\n",
							MUIC_DEV_NAME, __func__, ret, reg_val, qc20_temp);
					break;
#endif
				} else {
					afc_error_count++;
					/* ENAFC set '1' */
					sm5714_set_afc_ctrl_reg(muic_data,
						AFCCTRL_ENAFC, 1);
					pr_info("[%s:%s] AFC_ERROR, afc_error_count(%d)\n",
						MUIC_DEV_NAME, __func__, afc_error_count);
				}
			} else {
				pr_info("[%s:%s] AFC_ACCEPTED Fail(0x%x), retry(%d)\n",
						MUIC_DEV_NAME, __func__,
						irqafc, retry);
			}
		}

		/* ENAFC set '0' */
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

		/* AFC(INT2) unmask */
		intmask2 = intmask2 & 0xC4;
		sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_INTMASK2, intmask2);

		/* VBUS check */
		vbus_txd_voltage = 5 + ((afctxd&0xF0)>>4);
		voltage_min = vbus_txd_voltage - 2; /* - 2V */
		voltage_max = vbus_txd_voltage + 1; /* + 1V */

		vbus_check = 0;
		for (i = 0 ; i < 5 ; i++) {
			vbus_voltage = sm5714_muic_get_vbus_value(muic_data);
			pr_info("[%s:%s]i:%d TXD:%dV voltage_min:%dV voltage_max:%dV vbus_voltage:%dV\n",
					MUIC_DEV_NAME, __func__,
					i, vbus_txd_voltage,
					voltage_min, voltage_max, vbus_voltage);
			if ((voltage_min <= vbus_voltage) &&
				(vbus_voltage <= voltage_max)) {
				vbus_check = 1;
				i = 10; /* break */
			} else {
				msleep(100);
				pr_info("[%s:%s] retry:%d\n",
					MUIC_DEV_NAME, __func__, i);
			}
		}

		if (attached_dev == SM5714_MUIC_QC20) {
			if (vbus_check)
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_QC20, vbus_txd_voltage);
			else
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_QC20, 5);
		} else {
			if (vbus_check)
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_AFC_TA, vbus_txd_voltage);
			else
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_AFC_TA, 5);
		}
	}

	mutex_unlock(&muic_data->afc_mutex);

	return 0;
}

static void muic_afc_torch_work(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data = afc_init_data;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	if ((muic_data->fled_torch_enable == 1) || (muic_data->fled_flash_enable == 1)) {
		pr_info("[%s:%s] FLASH or Torch On, Skip 5V -> 9V\n",
			MUIC_DEV_NAME, __func__);
		return;
	}

	muic_disable_afc(0);  /* 5V -> 9V(12V) */
}

int sm5714_set_afc_ctrl_reg(struct sm5714_muic_data *muic_data, int shift,
		bool on)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val = 0;
	int ret = 0;

	pr_info("[%s:%s] Register[%d], set [%d]\n", MUIC_DEV_NAME, __func__,
			shift, on);
	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCCNTL);
	if (ret < 0)
		pr_err("[%s:%s](%d)\n", MUIC_DEV_NAME, __func__, ret);
	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		pr_debug("[%s:%s] reg_val(0x%x) != AFC_CTRL reg(0x%x), update reg\n",
			MUIC_DEV_NAME, __func__, reg_val, ret);

		ret = sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCCNTL,
				reg_val);
		if (ret < 0)
			pr_err("[%s:%s] err write AFC_CTRL(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
	} else {
		pr_debug("[%s:%s] (0x%x), just return\n",
			MUIC_DEV_NAME, __func__, ret);
		return 0;
	}

	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCCNTL);
	if (ret < 0)
		pr_err("[%s:%s] err read AFC_CTRL(%d)\n",
			MUIC_DEV_NAME, __func__, ret);
	else
		pr_debug("[%s:%s] AFC_CTRL reg after change(0x%x)\n",
			MUIC_DEV_NAME, __func__, ret);

	return ret;
}

int sm5714_afc_ta_attach(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0, afctxd = 0;
	int vbvolt = 0;
	union power_supply_propval value;
	int retry = 0;
	int dev1 = 0, dev2 = 0;

	pr_info("[%s:%s] AFC_TA_ATTACHED\n", MUIC_DEV_NAME, __func__);

	if (!afc_init_data->is_charger_ready) {
		pr_info("[%s:%s] charger is not ready, return\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}

	if (!afc_init_data->is_pdic_ready) {
		pr_info("[%s:%s] pdic is not ready, return\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}
#if IS_ENABLED(CONFIG_MUIC_LO_TA_LOW_CURRENT)
	if (muic_data->attached_dev == ATTACHED_DEV_LO_TA_MUIC) {
		pr_info("[%s:%s] Cable is LO_TA, return\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}
#endif
	if (muic_data->pdata->afc_disable) {
		pr_info("[%s:%s] AFC is disabled by USER, return\n",
				MUIC_DEV_NAME, __func__);
		muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		return ret;
	}

	if (muic_data->vbus_changed_9to5 == 1) {
		muic_data->vbus_changed_9to5 = 0;
#if defined(CONFIG_MUIC_QC_DISABLE)
		sm5714_afc_notifier_attach(muic_data, SM5714_MUIC_AFC_TA, 5);
#else
		sm5714_afc_notifier_attach(muic_data, SM5714_MUIC_QC20, 5);
#endif
		return 0;
	}

	/* read VBUS VALID */
	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS);
	if (ret < 0) {
		pr_err("[%s:%s] err read VBUS\n", MUIC_DEV_NAME, __func__);
		return 0;
	}
	pr_info("[%s:%s] VBUS[0x%02x]\n", MUIC_DEV_NAME, __func__, ret);

	vbvolt = (ret&0x04)>>2;
	if (!vbvolt) {
		pr_info("[%s:%s] VBUS NOT VALID [0x%02x] just return\n",
				MUIC_DEV_NAME, __func__, ret);
		return 0;
	}

	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE1);
	if (ret < 0) {
		pr_err("[%s:%s] err read DEVICE TYPE1\n", MUIC_DEV_NAME, __func__);
		return 0;
	}
	dev1 = ret;

	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE2);
	if (ret < 0) {
		pr_err("[%s:%s] err read DEVICE TYPE2\n", MUIC_DEV_NAME, __func__);
		return 0;
	}
	dev2 = ret;

	pr_info("[%s:%s] DEVICE_TYPE1 [0x%02x], DEVICE_TYPE2 [0x%02x]\n",
			MUIC_DEV_NAME, __func__, dev1, dev2);
	if ((dev1 == 0x00) && (dev2 == 0x00)) {
		pr_info("[%s:%s] No Device Type just return\n",
				MUIC_DEV_NAME, __func__);
		return 0;
	}

	/* read clear : AFC_STATUS */
	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCSTATUS);
	if (ret < 0)
		pr_err("[%s:%s] err read AFC_STATUS\n",
				MUIC_DEV_NAME, __func__);
	pr_info("[%s:%s] AFC_STATUS [0x%02x]\n", MUIC_DEV_NAME, __func__, ret);

	if (sm5714_is_afc_disabled(muic_data)) {
		pr_info("[%s:%s] Skip AFC\n",
			MUIC_DEV_NAME, __func__);
		muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		return 0;
	}

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	if (muic_data->pdic_afc_state == SM5714_MUIC_AFC_ABNORMAL) {
		muic_data->pdic_afc_state_count = 0;
		cancel_delayed_work(&muic_data->pdic_afc_work);
		cancel_delayed_work(&muic_data->afc_retry_work);
		schedule_delayed_work(&muic_data->pdic_afc_work,
				msecs_to_jiffies(200));
		pr_info("[%s:%s] PDIC Abnormal State, pdic_afc_work(%d) start\n",
				MUIC_DEV_NAME, __func__,
				muic_data->pdic_afc_state_count);

		return 0;
	}
#endif

	muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
	muic_notifier_attach_attached_dev(muic_data->attached_dev);

	/* Wait until less than 500mA.*/
	for (retry = 0; retry < 20; retry++) {
		usleep_range(5000, 5100);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		psy_do_property("sm5714-charger", get,
				POWER_SUPPLY_PROP_CURRENT_MAX, value);
#endif
		if (value.intval <= 500) {
			pr_info("[%s:%s]PREPARE Success(%d mA), retry(%d)\n",
					MUIC_DEV_NAME, __func__, value.intval, retry);
			break;
		}
		pr_info("[%s:%s]PREPARE fail(%d mA), retry(%d)\n", MUIC_DEV_NAME, __func__,
				value.intval, retry);
	}

	cancel_delayed_work(&muic_data->afc_retry_work);
	schedule_delayed_work(&muic_data->afc_retry_work,
			msecs_to_jiffies(5000)); /* 5sec */
	pr_info("[%s:%s] afc_retry_work(ATTACH) start\n",
			MUIC_DEV_NAME, __func__);

	/* voltage(9.0V)  + current(1.65A) setting : 0x46 */
	/* voltage(12.0V) + current(2.1A) setting  : 0x79 */
	afctxd = AFC_TXBYTE_9V_1_65A;
	ret = sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCTXD, afctxd);
	if (ret < 0)
		pr_err("[%s:%s] err write AFC_TXD(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
	pr_info("[%s:%s] AFC_TXD [0x%02x]\n", MUIC_DEV_NAME, __func__, afctxd);

	/* ENAFC set '1' */
	sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
	pr_info("[%s:%s] AFCCTRL_ENAFC 1\n", MUIC_DEV_NAME, __func__);
	muic_data->afc_retry_count = 0;
	return 0;
}

int sm5714_afc_ta_accept(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int dev1 = 0;
	int txd_voltage = 0, vbus_voltage = 0;
	int voltage_min = 0, voltage_max = 0;
	int i = 0, vbus_check = 0;
	bool afc_disabled = false;

	pr_info("[%s:%s] AFC_ACCEPTED\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->afc_mutex);

	/* ENAFC set '0' */
	sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

	cancel_delayed_work(&muic_data->afc_retry_work);
	pr_info("[%s:%s] afc_retry_work(ACCEPTED) cancel\n",
			MUIC_DEV_NAME, __func__);

	muic_data->vbus_changed_9to5 = 0;

	afc_disabled = sm5714_is_afc_disabled(muic_data);

	if (!afc_disabled) {
		dev1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE1);
		pr_info("[%s:%s] dev1 [0x%02x]\n", MUIC_DEV_NAME, __func__, dev1);
		if (dev1 & DEV_TYPE1_AFC_TA) {
			txd_voltage = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCTXD);
			txd_voltage = 5 + ((txd_voltage&0xF0)>>4);
			voltage_min  = txd_voltage - 2;
			voltage_max  = txd_voltage + 1;

			vbus_check = 0;
			for (i = 0 ; i < 5 ; i++) {
				vbus_voltage = sm5714_muic_get_vbus_value(muic_data);
				pr_info("[%s:%s]i:%d TXD:%dV voltage_min:%dV voltage_max:%dV vbus_voltage:%dV\n",
						MUIC_DEV_NAME, __func__, i, txd_voltage,
						voltage_min, voltage_max, vbus_voltage);
				if ((voltage_min <= vbus_voltage) &&
					(vbus_voltage <= voltage_max)) {
					vbus_check = 1;
					i = 10; /* break */
				} else {
					msleep(100);
					pr_info("[%s:%s] retry:%d\n",
						MUIC_DEV_NAME, __func__, i);
				}
			}
			if (vbus_check)
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_AFC_TA, txd_voltage);
			else
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_AFC_TA, 5);
		} else {
			sm5714_afc_notifier_attach(muic_data, SM5714_MUIC_AFC_TA, 5);
		}
	}

	mutex_unlock(&muic_data->afc_mutex);

	if (afc_disabled)
		muic_disable_afc(1); /* 9V(12V) -> 5V */

	return 0;
}

int sm5714_afc_multi_byte(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int multi_byte[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int i = 0;
	int ret = 0;
	int voltage_find = 0;
	int afc_rx_num = 0;

	pr_info("[%s:%s] AFC_MULTI_BYTE\n", MUIC_DEV_NAME, __func__);

	/* ENAFC set '0' */
	sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

	afc_rx_num = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFC_RX_NUM);
	pr_info("[%s:%s] afc_rx_num=%d\n", MUIC_DEV_NAME, __func__, afc_rx_num);

	/* read AFC_RXD1 ~ RXD15 */
	voltage_find = 0;
	for (i = 0 ; i < 16 ; i++) {
		multi_byte[i] = sm5714_i2c_read_byte(i2c,
				SM5714_MUIC_REG_AFC_RXD1 + i);
		if (multi_byte[i] < 0) {
			pr_err("[%s:%s] err read AFC_RXD%d %d\n",
				MUIC_DEV_NAME, __func__, i+1, multi_byte[i]);
		}
		pr_info("[%s:%s] AFC_RXD%d [0x%02x]\n", MUIC_DEV_NAME, __func__,
				i+1, multi_byte[i]);
		if (multi_byte[i] == 0x00)
			break;
		if (i >= 1) /* voltate find */
			if (((multi_byte[i]&0xF0)>>4) >=
					((multi_byte[voltage_find]&0xF0)>>4))
				voltage_find = i;
	}

	pr_info("[%s:%s] AFC_RXD%d multi_byte[%d]=0x%02x\n",
			MUIC_DEV_NAME, __func__,
			voltage_find+1, voltage_find, multi_byte[voltage_find]);

	ret = sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCTXD,
			multi_byte[voltage_find]);
	if (ret < 0)
		pr_err("[%s:%s] err write AFC_TXD(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

	/* ENAFC set '1' */
	sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
	pr_info("[%s:%s] AFCCTRL_ENAFC 1\n", MUIC_DEV_NAME, __func__);

	return 0;
}

int sm5714_afc_error(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int value = 0;
	int dev1 = 0;
	int val1 = 0, val2 = 0, val3 = 0;
#if !defined(CONFIG_MUIC_QC_DISABLE)
	int ret = 0, reg_val = 0;
	int txd_voltage = 0, vbus_voltage = 0;
	int voltage_min = 0, voltage_max = 0;
	int i = 0, vbus_check = 0;
#endif

	pr_info("[%s:%s] AFC_ERROR (%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->afc_retry_count);

	/* ENAFC set '0' */
	sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);

	/* read AFC_STATUS */
	value = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCSTATUS);
	if (value < 0)
		pr_err("[%s:%s] err read AFC_STATUS %d\n",
				MUIC_DEV_NAME, __func__, value);
	pr_info("[%s:%s] REG_AFCSTATUS [0x%02x]\n", MUIC_DEV_NAME, __func__,
			value);

	val1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFC_RX_P0);
	val2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFC_RX_P1);
	val3 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFC_STATE);
	pr_info("[%s:%s] AFC_RX_PARITY0[0x%02x], AFC_RX_PARITY1[0x%02x], AFC_STATE[0x%02x]\n",
		MUIC_DEV_NAME, __func__, val1, val2, val3);

	dev1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE1);
	pr_info("[%s:%s] DEVICE_TYPE1 [0x%02x]\n",
		MUIC_DEV_NAME, __func__, dev1);

	if (muic_data->vbus_changed_9to5 == 1) {
		pr_info("[%s:%s] VBUS error(9 -> 5) DP_RESET\n",
			MUIC_DEV_NAME, __func__);
		/* DP_RESET '1' */
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_DP_RESET, 1);
		return 0;
	}

#if defined(CONFIG_MUIC_QC_DISABLE)
	if (muic_data->afc_retry_count < 5) {
		msleep(100); /* 100ms delay */
		/* ENAFC set '1' */
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
		muic_data->afc_retry_count++;
		pr_info("[%s:%s] re-start AFC (afc_retry_count=%d)\n",
				MUIC_DEV_NAME, __func__,
				muic_data->afc_retry_count);
	} else {
		pr_info("[%s:%s] ENAFC end = %d\n", MUIC_DEV_NAME, __func__,
				muic_data->afc_retry_count);
		muic_data->attached_dev = ATTACHED_DEV_TA_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
	}
#else
	if (muic_data->afc_retry_count < 5) {
		if ((dev1 & DEV_TYPE1_QC20_TA) &&
				(muic_data->afc_retry_count >= 2)) {

			txd_voltage = sm5714_i2c_read_byte(i2c,
				SM5714_MUIC_REG_AFCTXD);
			txd_voltage = 5 + ((txd_voltage&0xF0)>>4);

			ret = sm5714_i2c_read_byte(i2c,
				SM5714_MUIC_REG_AFCCNTL);
			if (txd_voltage == 12) { /* QC20_12V_TA */
				reg_val = (ret & 0x3F) | (SM5714_ENQC20_12V<<6);
			} else if (txd_voltage == 9) { /* QC20_9V_TA */
				reg_val = (ret & 0x3F) | (SM5714_ENQC20_9V<<6);
			} else {
				reg_val = (ret & 0x3F) | (SM5714_ENQC20_5V<<6);
			}
			sm5714_i2c_write_byte(i2c,
				SM5714_MUIC_REG_AFCCNTL, reg_val);
			pr_info("[%s:%s] read REG_AFCCNTL=0x%x, write REG_AFCCNTL=0x%x\n",
				MUIC_DEV_NAME, __func__, ret, reg_val);

			msleep(100);

			voltage_min  = txd_voltage - 2;
			voltage_max  = txd_voltage + 1;

			vbus_check = 0;
			for (i = 0 ; i < 5 ; i++) {
				vbus_voltage =
					sm5714_muic_get_vbus_value(muic_data);
				pr_info("[%s:%s]i:%d TXD:%dV voltage_min:%dV voltage_max:%dV vbus_voltage:%dV\n",
						MUIC_DEV_NAME, __func__,
						i, txd_voltage,
						voltage_min, voltage_max,
						vbus_voltage);

				if ((voltage_min <= vbus_voltage) &&
					(vbus_voltage <= voltage_max)) {
					vbus_check = 1;
					i = 10; /* break */
				} else {
					msleep(100);
					pr_info("[%s:%s] retry:%d\n",
						MUIC_DEV_NAME, __func__, i);
				}
			}
			if (vbus_check)
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_QC20, txd_voltage);
			else {
				sm5714_afc_notifier_attach(muic_data,
					SM5714_MUIC_QC20, 5);

				ret = sm5714_i2c_read_byte(i2c,
					SM5714_MUIC_REG_AFCCNTL);
				reg_val = (ret & 0x3F) | (SM5714_ENQC20_5V<<6);
				sm5714_i2c_write_byte(i2c,
					SM5714_MUIC_REG_AFCCNTL, reg_val);
			}

		} else {
			msleep(100); /* 100ms delay */
			/* ENAFC set '1' */
			sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
			muic_data->afc_retry_count++;
			pr_info("[%s:%s] re-start AFC (afc_retry_count=%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->afc_retry_count);
		}
	} else {
		pr_info("[%s:%s] ENAFC end = %d\n", MUIC_DEV_NAME, __func__,
				muic_data->afc_retry_count);
		if (dev1 & DEV_TYPE1_QC20_TA)
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC;
		else
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
	}
#endif
	return 0;
}

int sm5714_afc_sta_chg(struct sm5714_muic_data *muic_data)
{
	pr_info("[%s:%s] AFC_STA_CHG (attached_dev: %d)\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);

	return 0;
}

int sm5714_muic_get_vbus_value(struct sm5714_muic_data *muic_data)
{
	int vbus_voltage = 0, vol = 0;

	vbus_voltage = sm5714_muic_get_vbus();

	if ((vbus_voltage > 4500) && (vbus_voltage <= 5500))
		vol = 5;
	else if ((vbus_voltage > 5500) && (vbus_voltage <= 6500))
		vol = 6;
	else if ((vbus_voltage > 6500) && (vbus_voltage <= 7500))
		vol = 7;
#if defined(CONFIG_SEC_FACTORY)
	else if ((vbus_voltage > 7500) && (vbus_voltage <= 9500))
		vol = 9;
#else
	else if ((vbus_voltage > 7500) && (vbus_voltage <= 8500))
		vol = 8;
	else if ((vbus_voltage > 8500) && (vbus_voltage <= 9500))
		vol = 9;
#endif
	else if ((vbus_voltage > 9500) && (vbus_voltage <= 10500))
		vol = 10;
	else if ((vbus_voltage > 10500) && (vbus_voltage <= 11500))
		vol = 11;
	else if ((vbus_voltage > 11500) && (vbus_voltage <= 12500))
		vol = 12;
	else
		vol = vbus_voltage/1000;

	pr_info("[%s:%s] VBUS:%dV\n", MUIC_DEV_NAME, __func__, vol);

	return vol;
}

static void sm5714_afc_notifier_attach(struct sm5714_muic_data *muic_data,
	int afcta, int txt_voltage)
{
	pr_info("[%s:%s] afcta:%d txt_voltage:%d\n", MUIC_DEV_NAME, __func__,
		afcta, txt_voltage);
	pr_info("[%s:%s] AFC_PPRPARE(%d), AFC_5V(%d), AFC_9V(%d), QC_5V(%d), QC_9V(%d)\n",
		MUIC_DEV_NAME, __func__,
		ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC,
		ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
		ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
		ATTACHED_DEV_QC_CHARGER_5V_MUIC,
		ATTACHED_DEV_QC_CHARGER_9V_MUIC);

	if (afcta == SM5714_MUIC_AFC_TA) { /* AFC TA */
		if (txt_voltage == 12) { /* 12V */
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_12V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
			pr_info("[%s:%s] AFC 12V (%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
		} else if (txt_voltage == 9) { /* 9V */
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
			pr_info("[%s:%s] AFC 9V (%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
		} else if (txt_voltage == 5) { /* 5V */
			muic_data->attached_dev =
				ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
			pr_info("[%s:%s] AFC 5V (%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
		}
	} else { /* QC20 TA */
		if (txt_voltage == 12) { /* 12V */
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
			pr_info("[%s:%s] QC20 12V (%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
		} else if (txt_voltage == 9) { /* 9V */
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
			pr_info("[%s:%s] QC20 9V (%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
		} else if (txt_voltage == 5) {
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_5V_MUIC;
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
			pr_info("[%s:%s] QC20 5V (%d)\n",
					MUIC_DEV_NAME, __func__,
					muic_data->attached_dev);
		}
	}
}

static void hv_muic_change_afc_voltage(int tx_data)
{
	struct sm5714_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	u8 val = 0;
	union power_supply_propval value;
	int retry = 0;	

	pr_info("[%s:%s] change afc voltage(%x)\n",
			MUIC_DEV_NAME, __func__, tx_data);

	mutex_lock(&muic_data->afc_mutex);

	/* QC20 */
	if ((muic_data->attached_dev == ATTACHED_DEV_QC_CHARGER_9V_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_QC_CHARGER_5V_MUIC)) {
		switch (tx_data) {
		case SM5714_MUIC_HV_5V:
			/* QC20 5V */
			sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_QC20_9V, 0);
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_5V_MUIC;
			muic_notifier_attach_attached_dev(
				muic_data->attached_dev);
			break;
		case SM5714_MUIC_HV_9V:
			/* QC20 9V */
			sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_QC20_9V, 1);
			muic_data->attached_dev =
				ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			muic_notifier_attach_attached_dev(
				muic_data->attached_dev);
			break;
		default:
			break;
		}
	} else {	/* AFC */
		val = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCTXD);
		if (val == tx_data) {
			pr_info("[%s:%s] same to current voltage 0x%x\n",
					MUIC_DEV_NAME, __func__, val);
			goto EOH;
		}

		muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		
		/* Wait until less than 500mA.*/
		for (retry = 0; retry < 20; retry++) {
			usleep_range(5000, 5100);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			psy_do_property("sm5714-charger", get,
					POWER_SUPPLY_PROP_CURRENT_MAX, value);
#endif
			if (value.intval <= 500) {
				pr_info("[%s:%s]PREPARE Success(%d mA), retry(%d)\n",
						MUIC_DEV_NAME, __func__, value.intval, retry);
				break;
			}
			pr_info("[%s:%s]PREPARE fail(%d mA), retry(%d)\n", MUIC_DEV_NAME, __func__,
					value.intval, retry);
		}

		cancel_delayed_work(&muic_data->afc_retry_work);
		schedule_delayed_work(&muic_data->afc_retry_work,
				msecs_to_jiffies(5000)); /* 5sec */
		pr_info("[%s:%s] afc_retry_work(afc voltage) start\n",
				MUIC_DEV_NAME, __func__);

		sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCTXD, tx_data);
		/* ENAFC set '1' */
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
		muic_data->afc_retry_count = 0;
	}

EOH:
	mutex_unlock(&muic_data->afc_mutex);
}

int sm5714_muic_afc_set_voltage(int vol)
{
	int now_vol = 0;
	struct sm5714_muic_data *muic_data = afc_init_data;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		break;
	default:
		pr_info("[%s:%s] not HV charger, returnV\n",
				MUIC_DEV_NAME, __func__);
		return -EINVAL;
	}

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		now_vol = 9;
		break;
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
		now_vol = 5;
		break;
	default:
		break;
	}

	if (now_vol == vol) {
		pr_info("[%s:%s] same voltage(%d), return\n",
				MUIC_DEV_NAME, __func__, vol);
		return 0;
	}

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	/* do not set high voltage at below conditions */
	if (vol > 5) {
		if (sm5714_is_afc_disabled(muic_data)) {
			pr_info("[%s:%s] Skip AFC\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}

		if (muic_data->pdic_afc_state == SM5714_MUIC_AFC_ABNORMAL) {
			pr_info("[%s:%s] pdic abnormal: AFC(QC20) skip\n",
					MUIC_DEV_NAME, __func__);
			return 0;
		}
	}
#endif

	pr_info("[%s:%s] vol = %dV\n", MUIC_DEV_NAME, __func__, vol);
	muic_data->hv_voltage = vol;

	if (vol == 5) {
		if ((muic_data->attached_dev ==
				ATTACHED_DEV_AFC_CHARGER_9V_MUIC) ||
			(muic_data->attached_dev ==
				ATTACHED_DEV_AFC_CHARGER_12V_MUIC))
			muic_data->vbus_changed_9to5 = 1;

		hv_muic_change_afc_voltage(SM5714_MUIC_HV_5V);
	} else if (vol == 9) {
		hv_muic_change_afc_voltage(SM5714_MUIC_HV_9V);
	} else if (vol == 12) {
		hv_muic_change_afc_voltage(SM5714_MUIC_HV_12V);
	} else {
		pr_warn("[%s:%s]invalid value\n", MUIC_DEV_NAME, __func__);
		return -EINVAL;
	}

	return 0;
}

static void muic_afc_retry_work(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data = afc_init_data;
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0, vbvolt = 0;

	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCSTATUS);
	pr_info("[%s:%s]: Read REG_AFCSTATUS = [0x%02x]\n",
			MUIC_DEV_NAME, __func__, ret);

	pr_info("[%s:%s] attached_dev = %d\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	if (muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) {
		vbvolt = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS);
		vbvolt = (vbvolt&0x04)>>2;
		if (!vbvolt) {
			pr_info("[%s:%s] VBUS is nothing\n",
					MUIC_DEV_NAME, __func__);
			muic_notifier_detach_attached_dev(
					muic_data->attached_dev);
			muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
			return;
		}

		pr_info("[%s:%s] [MUIC] device type is afc prepare, DP_RESET\n",
				MUIC_DEV_NAME, __func__);

		/* DP_RESET '1' */
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_DP_RESET, 1);
	}
}

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
static void sm5714_muic_pdic_afc_handler(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data =
		container_of(work, struct sm5714_muic_data, pdic_afc_work.work);

	pr_info("[%s:%s] pdic_afc_state:%d, pdic_afc_state_count:%d\n",
			MUIC_DEV_NAME, __func__, muic_data->pdic_afc_state,
			muic_data->pdic_afc_state_count);

	if (muic_data->pdic_afc_state_count > 4) {
		cancel_delayed_work(&muic_data->pdic_afc_work);
		muic_data->attached_dev = ATTACHED_DEV_TA_MUIC;
		muic_notifier_attach_attached_dev(muic_data->attached_dev);
		return;
	}

	if (muic_data->pdic_afc_state == SM5714_MUIC_AFC_NORMAL) {
		pr_info("[%s:%s] SM5714_MUIC_AFC_NORMAL\n",
				MUIC_DEV_NAME, __func__);
		/* ENAFC set '1' */
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 1);
		pr_info("[%s:%s] AFCCTRL_ENAFC 1\n", MUIC_DEV_NAME, __func__);
		muic_data->afc_retry_count = 0;
	} else {
		muic_data->pdic_afc_state_count++;
		cancel_delayed_work(&muic_data->pdic_afc_work);
		schedule_delayed_work(&muic_data->pdic_afc_work,
				msecs_to_jiffies(200));
		pr_info("[%s:%s] pdic_afc_work(%d) start\n", MUIC_DEV_NAME,
				__func__, muic_data->pdic_afc_state_count);
	}
}
#endif

static void sm5714_hv_muic_init_detect(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data = afc_init_data;
	int afc_ta_attached = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	afc_ta_attached = sm5714_i2c_read_byte(muic_data->i2c,
			SM5714_MUIC_REG_DEVICETYPE2);
	pr_info("[%s:%s] HVDCP:[0x%02x]\n", MUIC_DEV_NAME, __func__,
			afc_ta_attached);

	/* AFC_TA_ATTACHED */
	if (afc_ta_attached & 0x40)
		sm5714_afc_ta_attach(muic_data);
}

int sm5714_muic_charger_init(void)
{
	int ret = -EINVAL;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	if (!afc_init_data) {
		pr_info("[%s:%s] MUIC AFC is not ready.\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}

	if (afc_init_data->is_charger_ready) {
		pr_info("[%s:%s] charger is already ready.\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}

	afc_init_data->is_charger_ready = true;

	if (!afc_init_data->is_pdic_ready) {
		pr_info("[%s:%s] pdic is not ready.\n",
				MUIC_DEV_NAME, __func__);
		return ret;
	}

	if ((afc_init_data->attached_dev == ATTACHED_DEV_TA_MUIC) ||
			(afc_init_data->attached_dev == ATTACHED_DEV_UNOFFICIAL_TA_MUIC))
		schedule_work(&afc_init_data->muic_afc_init_work);

	return 0;
}

void sm5714_hv_muic_initialize(struct sm5714_muic_data *muic_data)
{
	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	afc_init_data = muic_data;

	afc_init_data->is_charger_ready = false;

	/* To make AFC work properly on boot */
	INIT_WORK(&(muic_data->muic_afc_init_work), sm5714_hv_muic_init_detect);

	INIT_DELAYED_WORK(&muic_data->afc_retry_work, muic_afc_retry_work);
	INIT_DELAYED_WORK(&muic_data->afc_torch_work, muic_afc_torch_work);

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	INIT_DELAYED_WORK(&muic_data->pdic_afc_work,
			sm5714_muic_pdic_afc_handler);
#endif
}
