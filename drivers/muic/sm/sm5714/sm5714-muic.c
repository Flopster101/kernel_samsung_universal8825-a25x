/*
 * driver/muic/sm5714-muic.c - SM5714 micro USB switch device driver
 *
 * Copyright (C) 2017 SiliconMitus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 */

#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/pm_wakeup.h>
#include <linux/switch.h>

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && !defined(CONFIG_BATTERY_GKI)
#include <linux/sec_batt.h>
#endif

#if IS_BUILTIN(CONFIG_MUIC_SM5714)
#if IS_ENABLED(CONFIG_ARCH_QCOM) && !defined(CONFIG_USB_ARCH_EXYNOS) && !defined(CONFIG_ARCH_EXYNOS)
#include <linux/sec_param.h>
#else
#include <linux/sec_ext.h>
#endif
#endif

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#include <linux/muic/common/muic_param.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb_notify.h>
#endif /* CONFIG_PDIC_NOTIFIER */
#include <linux/usb/typec/common/pdic_param.h>

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif /* CONFIG_VBUS_NOTIFIER */

#include <linux/muic/common/muic.h>
#include <linux/mfd/sm/sm5714/sm5714.h>
#include <linux/mfd/sm/sm5714/sm5714-private.h>
#include <linux/muic/sm/sm5714/sm5714-muic.h>

#if IS_ENABLED(CONFIG_CP_UART_NOTI)
#include <soc/samsung/exynos-modem-ctrl.h>
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "../../../battery/common/sec_charging_common.h"
#endif


static struct sm5714_muic_data *static_data;

static int com_to_open(struct sm5714_muic_data *muic_data);
static void sm5714_muic_handle_attach(struct sm5714_muic_data *muic_data,
			int new_dev, u8 vbvolt, int irq);
static void sm5714_muic_handle_detach(struct sm5714_muic_data *muic_data,
			int irq);
static void sm5714_muic_detect_dev(struct sm5714_muic_data *muic_data, int irq);
static int sm5714_muic_get_adc(struct sm5714_muic_data *muic_data);
static int switch_to_ap_uart(struct sm5714_muic_data *muic_data, int new_dev);
static int switch_to_cp_uart(struct sm5714_muic_data *muic_data, int new_dev);

static void sm5714_muic_delayed_523Kto619K_work(struct work_struct *work);

char *SM5714_MUIC_INT_NAME[11] = {
	"DPDM_OVP",			/* 0 */
	"VBUS_RID_DETACH",	/* 1 */
	"RID_DETECT",		/* 2 */
	"CHGTYPE",			/* 3 */
	"DCDTIMEOUT",		/* 4 */
	"AFC_ERROR",		/* 5 */
	"AFC_STA_CHG",		/* 6 */
	"MULTI_BYTE",		/* 7 */
	"VBUS_UPDATE",		/* 8 */
	"AFC_ACCEPTED",		/* 9 */
	"AFC_TA_ATTACHED"	/* 10 */
};

/* #define DEBUG_MUIC */

#if defined(DEBUG_MUIC)
#define MAX_LOG 25
#define READ 0
#define WRITE 1

static u8 sm5714_log_cnt;
static u8 sm5714_log[MAX_LOG][3];

static void sm5714_reg_log(u8 reg, u8 value, u8 rw)
{
	sm5714_log[sm5714_log_cnt][0] = reg;
	sm5714_log[sm5714_log_cnt][1] = value;
	sm5714_log[sm5714_log_cnt][2] = rw;
	sm5714_log_cnt++;
	if (sm5714_log_cnt >= MAX_LOG)
		sm5714_log_cnt = 0;
}

static void sm5714_print_reg_log(void)
{
	int i = 0;
	u8 reg = 0, value = 0, rw = 0;
	char mesg[256] = "";

	for (i = 0; i < MAX_LOG; i++) {
		reg = sm5714_log[sm5714_log_cnt][0];
		value = sm5714_log[sm5714_log_cnt][1];
		rw = sm5714_log[sm5714_log_cnt][2];
		sm5714_log_cnt++;

		if (sm5714_log_cnt >= MAX_LOG)
			sm5714_log_cnt = 0;
		sprintf(mesg+strlen(mesg), "%x(%x)%x ", reg, value, rw);
	}
	pr_info("[%s:%s] %s\n", MUIC_DEV_NAME, __func__, mesg);
}

void sm5714_read_reg_dump(struct sm5714_muic_data *muic, char *mesg)
{
	u8 val = 0;

	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_INTMASK1, &val);
	sprintf(mesg+strlen(mesg), "IM1:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_INTMASK2, &val);
	sprintf(mesg+strlen(mesg), "IM2:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_CNTL, &val);
	sprintf(mesg+strlen(mesg), "CTRL:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_MANUAL_SW, &val);
	sprintf(mesg+strlen(mesg), "SW:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_DEVICETYPE1, &val);
	sprintf(mesg+strlen(mesg), "DT1:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_DEVICETYPE2, &val);
	sprintf(mesg+strlen(mesg), "DT2:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_AFCCNTL, &val);
	sprintf(mesg+strlen(mesg), "AFC_CTRL:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_AFCTXD, &val);
	sprintf(mesg+strlen(mesg), "AFC_TXD:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_AFCSTATUS, &val);
	sprintf(mesg+strlen(mesg), "AFC_STATUS:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_VBUS_VOLTAGE, &val);
	sprintf(mesg+strlen(mesg), "VOL:%x ", val);
	sm5714_read_reg(muic->i2c, SM5714_MUIC_REG_AFC_STATE, &val);
	sprintf(mesg+strlen(mesg), "AFC_STA:%x ", val);
	sm5714_read_reg(muic->i2c, 0x38, &val);
	sprintf(mesg+strlen(mesg), "0x38:%x ", val);
	sm5714_read_reg(muic->i2c, 0x42, &val);
	sprintf(mesg+strlen(mesg), "0x42:%x ", val);
	sm5714_read_reg(muic->i2c, 0x58, &val);
	sprintf(mesg+strlen(mesg), "0x58:%x ", val);
}

void sm5714_print_reg_dump(struct sm5714_muic_data *muic_data)
{
	char mesg[1024] = "";

	sm5714_read_reg_dump(muic_data, mesg);

	pr_info("[%s:%s] %s\n", MUIC_DEV_NAME, __func__, mesg);
}
#else
void sm5714_print_reg_dump(struct sm5714_muic_data *muic_data)
{
}
#endif

int sm5714_i2c_read_byte(struct i2c_client *client, u8 command)
{
	u8 ret = 0;

	sm5714_read_reg(client, command, &ret);
	return ret;
}

int sm5714_i2c_write_byte(struct i2c_client *client,
			u8 command, u8 value)
{
	int ret = 0;
	int retry = 0;
	u8 written = 0;

	ret = sm5714_write_reg(client, command, value);

	while (ret < 0) {
		pr_info("[%s:%s] reg(0x%x), retrying...\n",
			MUIC_DEV_NAME, __func__, command);
		sm5714_read_reg(client, command, &written);
		if (written != value)
			pr_err("[%s:%s] reg(0x%x)\n",
				MUIC_DEV_NAME, __func__, command);
		msleep(100);
		ret = sm5714_write_reg(client, command, value);
		retry++;
	}
#ifdef DEBUG_MUIC
	sm5714_reg_log(command, value, retry << 1 | WRITE);
#endif
	return ret;
}
static int sm5714_i2c_guaranteed_wbyte(struct i2c_client *client,
			u8 command, u8 value)
{
	int ret = 0;
	int retry = 0;
	int written = 0;

	ret = sm5714_i2c_write_byte(client, command, value);
	written = sm5714_i2c_read_byte(client, command);

	while (written != value) {
		pr_info("[%s:%s] reg(0x%x): written(0x%x) != value(0x%x)\n",
			MUIC_DEV_NAME, __func__, command, written, value);
		if (retry > 10) {
			pr_err("[%s:%s] retry failed!!\n", MUIC_DEV_NAME,
					__func__);
			break;
		}
		msleep(100);
		retry++;
		ret = sm5714_i2c_write_byte(client, command, value);
		written = sm5714_i2c_read_byte(client, command);
	}
	return ret;
}

static ssize_t sm5714_muic_show_uart_en(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = 0;

	if (!muic_data->is_rustproof) {
		pr_info("[%s:%s] UART ENABLE\n",  MUIC_DEV_NAME, __func__);
		ret = sprintf(buf, "1\n");
	} else {
		pr_info("[%s:%s] UART DISABLE\n",  MUIC_DEV_NAME, __func__);
		ret = sprintf(buf, "0\n");
	}

	return ret;
}

static ssize_t sm5714_muic_set_uart_en(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1))
		muic_data->is_rustproof = false;
	else if (!strncmp(buf, "0", 1))
		muic_data->is_rustproof = true;
	else
		pr_info("[%s:%s] invalid value\n",  MUIC_DEV_NAME, __func__);

	pr_info("[%s:%s] uart_en(%d)\n",
		MUIC_DEV_NAME, __func__, !muic_data->is_rustproof);

	return count;
}

#ifndef CONFIG_UART_SWITCH
static ssize_t sm5714_muic_show_uart_sel(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		mode = "AP\n";
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP\n";
		break;
	default:
		break;
	}

	pr_info("[%s:%s] uart_sel(%s)\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s", mode);
}

static ssize_t sm5714_muic_set_uart_sel(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data, muic_data->attached_dev);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data, muic_data->attached_dev);
	} else {
		pr_info("[%s:%s] invalid value\n",  MUIC_DEV_NAME, __func__);
	}

	pr_info("[%s:%s] uart_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->uart_path);

	return count;
}

static ssize_t sm5714_muic_show_usb_sel(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		mode = "PDA\n";
		break;
	case MUIC_PATH_USB_CP:
		mode = "MODEM\n";
		break;
	default:
		break;
	}

	pr_info("[%s:%s] usb_sel(%s)\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s", mode);
}

static ssize_t sm5714_muic_set_usb_sel(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3))
		pdata->usb_path = MUIC_PATH_USB_AP;
	else if (!strncasecmp(buf, "MODEM", 5))
		pdata->usb_path = MUIC_PATH_USB_CP;
	else
		pr_info("[%s:%s] invalid value\n",  MUIC_DEV_NAME, __func__);

	pr_info("[%s:%s] usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	return count;
}
#endif

static ssize_t sm5714_muic_show_adc(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&muic_data->muic_mutex);

	if (muic_data->is_factory_start)
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
		ret = usbpd_sbu_test_read(muic_data->man);
#else
		ret = 0;
#endif
	else
		ret = sm5714_muic_get_adc(muic_data);

	pr_info("[%s:%s] attached_dev: %d, adc = %d, is_factory_start = %d\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev,
			ret, muic_data->is_factory_start);

	mutex_unlock(&muic_data->muic_mutex);
	if (ret < 0) {
		pr_err("[%s:%s] err read adc reg(%d)\n",
			MUIC_DEV_NAME, __func__, ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & 0x1F));
}

static ssize_t sm5714_muic_show_usb_state(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return 0;
}

#ifdef DEBUG_MUIC
static ssize_t sm5714_muic_show_mansw(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&muic_data->muic_mutex);
	ret = sm5714_i2c_read_byte(muic_data->i2c, SM5714_MUIC_REG_MANUAL_SW);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("[%s:%s] manual sw:%d buf%s\n", MUIC_DEV_NAME, __func__,
			ret, buf);

	if (ret < 0) {
		pr_err("[%s:%s] fail to read muic reg\n", MUIC_DEV_NAME,
				__func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "0x%x\n", (ret&0x3F));
}

static ssize_t sm5714_muic_show_registers(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	char mesg[1024] = "";

	mutex_lock(&muic_data->muic_mutex);
	sm5714_read_reg_dump(muic_data, mesg);
	mutex_unlock(&muic_data->muic_mutex);
	pr_info("[%s:%s] %s\n", MUIC_DEV_NAME, __func__, mesg);

	return sprintf(buf, "%s\n", mesg);
}
#endif

#if defined(CONFIG_USB_HOST_NOTIFY)
static ssize_t sm5714_muic_show_otg_test(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	pr_info("[%s:%s] buf%s\n", MUIC_DEV_NAME, __func__, buf);

	return sprintf(buf, "\n");
}

static ssize_t sm5714_muic_set_otg_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("[%s:%s] buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/* The otg_test is set 0 durring the otg test. Not 1!!! */
	if (!strncmp(buf, "0", 1)) {
		muic_data->is_otg_test = 1;
	} else if (!strncmp(buf, "1", 1)) {
		muic_data->is_otg_test = 0;
	} else {
		pr_info("[%s:%s] Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	return count;
}
#endif

static ssize_t sm5714_muic_show_attached_dev(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	int mdev = muic_data->attached_dev;

	pr_info("[%s:%s] attached_dev:%d\n", MUIC_DEV_NAME, __func__,	mdev);

	switch (mdev) {
	case ATTACHED_DEV_NONE_MUIC:
		return sprintf(buf, "No VPS\n");
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB\n");
	case ATTACHED_DEV_CDP_MUIC:
		return sprintf(buf, "CDP\n");
	case ATTACHED_DEV_OTG_MUIC:
		return sprintf(buf, "OTG\n");
	case ATTACHED_DEV_TA_MUIC:
		return sprintf(buf, "TA\n");
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		return sprintf(buf, "JIG UART OFF\n");
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		return sprintf(buf, "JIG UART OFF/VB\n");
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		return sprintf(buf, "JIG UART ON\n");
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		return sprintf(buf, "JIG UART ON/VB\n");
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		return sprintf(buf, "JIG USB OFF\n");
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "JIG USB ON\n");
	case ATTACHED_DEV_DESKDOCK_MUIC:
		return sprintf(buf, "DESKDOCK\n");
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		return sprintf(buf, "AUDIODOCK\n");
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		return sprintf(buf, "PS CABLE\n");
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		return sprintf(buf, "AFC Charger\n");
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		return sprintf(buf, "DCD Timeout\n");
	default:
		break;
	}

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t sm5714_muic_show_audio_path(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return 0;
}

static ssize_t sm5714_muic_set_audio_path(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return 0;
}

static ssize_t sm5714_muic_show_apo_factory(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("[%s:%s] %s\n",
		MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t sm5714_muic_set_apo_factory(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	pr_info("[%s:%s] buf:%s\n",
		MUIC_DEV_NAME, __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
		muic_data->is_factory_start = true;
		mode = "FACTORY_MODE";
	} else {
		pr_info("[%s:%s] Wrong command\n",  MUIC_DEV_NAME, __func__);
		return count;
	}

	return count;
}

static ssize_t muic_show_vbus_value(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	int vol = 0;

	mutex_lock(&muic_data->afc_mutex);
	vol = sm5714_muic_get_vbus_value(muic_data);
	mutex_unlock(&muic_data->afc_mutex);

	return sprintf(buf, "%dV\n", vol);
}

static ssize_t sm5714_muic_show_afc_disable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (pdata->afc_disable) {
		pr_info("[%s:%s] AFC DISABLE\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("[%s:%s] AFC ENABLE", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "0\n");
}

static ssize_t sm5714_muic_set_afc_disable(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	int param_val;
	int ret;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval psy_val;
#endif

	if (!strncasecmp(buf, "1", 1)) {
		pdata->afc_disable = true;
		muic_afc_request_cause_clear();
	} else if (!strncasecmp(buf, "0", 1))
		pdata->afc_disable = false;
	else
		pr_warn("[%s:%s] invalid value\n", MUIC_DEV_NAME, __func__);

	param_val = (pdata->afc_disable) ? '1' : '0';
	ret = 0;

#if IS_BUILTIN(CONFIG_MUIC_SM5714)
#ifdef CONFIG_SEC_PARAM
#if IS_ENABLED(CONFIG_ARCH_QCOM) && !defined(CONFIG_USB_ARCH_EXYNOS) && !defined(CONFIG_ARCH_EXYNOS)
	ret = sec_set_param(param_index_afc_disable, &param_val);
	if (ret == false)
		pr_err("[%s:%s] set_param failed(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
#else
	ret = sec_set_param(CM_OFFSET + 1, param_val);
	if (ret < 0)
		pr_err("[%s:%s] set_param failed(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
#endif
#else /* CONFIG_SEC_PARAM */
	pr_err("%s:set_param is NOT supported!\n", __func__);
#endif
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_val.intval = param_val;
	psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
#endif

	pr_info("[%s:%s] attached_dev(%d), afc_disable(%d)\n",
		MUIC_DEV_NAME, __func__, muic_data->attached_dev, pdata->afc_disable);

	if (pdata->afc_disable) {
		if (muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_9V_MUIC ||
			muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC ||
			muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_5V_MUIC ||
			muic_data->attached_dev == ATTACHED_DEV_QC_CHARGER_9V_MUIC ||
			muic_data->attached_dev == ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC ||
			muic_data->attached_dev == ATTACHED_DEV_QC_CHARGER_5V_MUIC ||
			muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC) {
			sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_DP_RESET, 1);  /*DP_RESET*/
			msleep(50);

			muic_data->attached_dev = ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC;
			muic_notifier_attach_attached_dev(muic_data->attached_dev);
		}
	} else {
		if (muic_data->attached_dev == ATTACHED_DEV_TA_MUIC ||
			muic_data->attached_dev == ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC)
			sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_DP_RESET, 1);  /*DP_RESET*/
	}

	return count;
}

static ssize_t sm5714_afc_set_voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_info("%s+\n", __func__);
	if (!strncasecmp(buf, "5V", 2))
		muic_afc_set_voltage(0x5);
	else if (!strncasecmp(buf, "9V", 2))
		muic_afc_set_voltage(0x9);
	else
		pr_warn("%s invalid value : %s\n", __func__, buf);

	pr_info("%s-\n", __func__);
	return count;
}

#if defined(CONFIG_HICCUP_CHARGER)
static ssize_t hiccup_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "ENABLE\n");
}

static ssize_t hiccup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncasecmp(buf, "DISABLE", 7)) {
		pr_info("%s\n", __func__);
#if IS_ENABLED(CONFIG_IF_CB_MANAGER) && IS_ENABLED(CONFIG_HICCUP_CC_DISABLE)
		usbpd_cc_control_command(muic_data->man, 0);
#endif
		muic_data->is_hiccup_mode = false;
		com_to_open(muic_data);
	} else
		pr_warn("%s invalid com : %s\n", __func__, buf);

	return count;
}
#endif

static DEVICE_ATTR(uart_en, 0664, sm5714_muic_show_uart_en,
					sm5714_muic_set_uart_en);
#ifndef CONFIG_UART_SWITCH
static DEVICE_ATTR(uart_sel, 0664, sm5714_muic_show_uart_sel,
					sm5714_muic_set_uart_sel);
#endif
static DEVICE_ATTR(adc, 0664, sm5714_muic_show_adc, NULL);
#ifdef DEBUG_MUIC
static DEVICE_ATTR(mansw, 0664, sm5714_muic_show_mansw, NULL);
static DEVICE_ATTR(dump_registers, 0664, sm5714_muic_show_registers, NULL);
#endif
static DEVICE_ATTR(usb_state, 0444, sm5714_muic_show_usb_state, NULL);
#if defined(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664,
		sm5714_muic_show_otg_test, sm5714_muic_set_otg_test);
#endif
static DEVICE_ATTR(attached_dev, 0444, sm5714_muic_show_attached_dev, NULL);
static DEVICE_ATTR(audio_path, 0664,
		sm5714_muic_show_audio_path, sm5714_muic_set_audio_path);
static DEVICE_ATTR(apo_factory, 0664,
		sm5714_muic_show_apo_factory,
		sm5714_muic_set_apo_factory);
#ifndef CONFIG_UART_SWITCH
static DEVICE_ATTR(usb_sel, 0664,
		sm5714_muic_show_usb_sel,
		sm5714_muic_set_usb_sel);
#endif
static DEVICE_ATTR(vbus_value, 0444, muic_show_vbus_value, NULL);

static DEVICE_ATTR(afc_disable, 0664,
		sm5714_muic_show_afc_disable, sm5714_muic_set_afc_disable);

static DEVICE_ATTR(afc_set_voltage, 0220,
		NULL, sm5714_afc_set_voltage);
#if defined(CONFIG_HICCUP_CHARGER)
static DEVICE_ATTR_RW(hiccup);
#endif


static struct attribute *sm5714_muic_attributes[] = {
	&dev_attr_uart_en.attr,
#ifndef CONFIG_UART_SWITCH
	&dev_attr_uart_sel.attr,
#endif
	&dev_attr_adc.attr,
#ifdef DEBUG_MUIC
	&dev_attr_mansw.attr,
	&dev_attr_dump_registers.attr,
#endif
	&dev_attr_usb_state.attr,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_attached_dev.attr,
	&dev_attr_audio_path.attr,
	&dev_attr_apo_factory.attr,
#ifndef CONFIG_UART_SWITCH
	&dev_attr_usb_sel.attr,
#endif
	&dev_attr_vbus_value.attr,
	&dev_attr_afc_disable.attr,
	&dev_attr_afc_set_voltage.attr,
#if defined(CONFIG_HICCUP_CHARGER)
	&dev_attr_hiccup.attr,
#endif
	NULL
};

static const struct attribute_group sm5714_muic_group = {
	.attrs = sm5714_muic_attributes,
};

static int set_manual_sw(struct sm5714_muic_data *muic_data, bool on)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val = 0;
	int ret = 0;

	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_MANUAL_SW);
	if (ret < 0)
		pr_err("[%s:%s] err read MANUAL_SW(%d)\n",
			MUIC_DEV_NAME, __func__, ret);

	pr_info("[%s:%s] on:0x%x, cfg1=0x%x\n", MUIC_DEV_NAME, __func__,
			on, ret);

	if (on) /* MANSW_AUTOMATIC:0  MANSW_MANUAL:1 */
		reg_val = ret | 0x80;
	else
		reg_val = ret & 0x7F;

	pr_info("[%s:%s] reg_val(0x%x) != ret(0x%x) update\n",
		MUIC_DEV_NAME, __func__, reg_val, ret);

	ret = sm5714_i2c_guaranteed_wbyte(i2c, SM5714_MUIC_REG_MANUAL_SW,
			reg_val);
	if (ret < 0)
		pr_err("[%s:%s] err write(%d)\n",
			MUIC_DEV_NAME, __func__, ret);

	ret = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_MANUAL_SW);
	if (ret < 0)
		pr_err("[%s:%s] err read MANUAL_SW(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
	else
		pr_info("[%s:%s] after change(0x%x)\n",
			MUIC_DEV_NAME, __func__, ret);

	return ret;
}

static int get_com_sw(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int temp = 0;

	temp = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_MANUAL_SW);

	pr_info("[%s:%s] 0x%x\n", MUIC_DEV_NAME, __func__, temp);

	return (temp&0x3F);
}

static int set_com_sw(struct sm5714_muic_data *muic_data,
			enum sm5714_reg_manual_sw_value reg_val)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int temp = 0;

	/*  --- MANSW [5:3][2:0] : DM DP  --- */
	temp = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_MANUAL_SW);
	if (temp < 0)
		pr_err("[%s:%s] err read MANSW(0x%x)\n",
			MUIC_DEV_NAME, __func__, temp);

	pr_info("[%s:%s]rev_val(0x%x) != temp(0x%x), update\n",
		MUIC_DEV_NAME, __func__, reg_val, temp);

	temp = (temp&0xC0)|reg_val;

	ret = sm5714_i2c_guaranteed_wbyte(i2c,
		SM5714_MUIC_REG_MANUAL_SW, temp);
	if (ret < 0)
		pr_err("[%s:%s] err write MANSW(0x%x)\n",
			MUIC_DEV_NAME, __func__, reg_val);

	return ret;
}

static int com_to_open(struct sm5714_muic_data *muic_data)
{
	enum sm5714_reg_manual_sw_value reg_val;
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);
	reg_val = MANSW_OPEN;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("[%s:%s] set_com_sw err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, MANSW_MANUAL); /* auto:0  manual:1 */

	return ret;
}

static int com_to_usb(struct sm5714_muic_data *muic_data)
{
	enum sm5714_reg_manual_sw_value reg_val;
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);
	reg_val = MANSW_USB;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("[%s:%s]set_com_usb err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, MANSW_MANUAL); /* auto:0  manual:1 */

	return ret;
}

static int com_to_uart(struct sm5714_muic_data *muic_data)
{
	enum sm5714_reg_manual_sw_value reg_val;
	int ret = 0;

	if (muic_data->is_rustproof) {
		pr_info("[%s:%s] rustproof mode\n", MUIC_DEV_NAME, __func__);
		return ret;
	}
	reg_val = MANSW_UART;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("[%s:%s] set_com_uart err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, MANSW_MANUAL); /* auto:0  manual:1 */

	return ret;
}

static int com_to_JTAG(struct sm5714_muic_data *muic_data)
{
	enum sm5714_reg_manual_sw_value reg_val;
	int ret = 0;

	if (muic_data->is_rustproof) {
		pr_info("[%s:%s] rustproof mode\n", MUIC_DEV_NAME, __func__);
		return ret;
	}
	reg_val = MANSW_JTAG;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("[%s:%s] com_to_Jtag err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, MANSW_MANUAL); /* auto:0  manual:1 */

	return ret;
}

#if defined(CONFIG_HICCUP_CHARGER)
static int com_to_GND(struct sm5714_muic_data *muic_data)
{
	enum sm5714_reg_manual_sw_value reg_val;
	int ret = 0;

	reg_val = MANSW_HICCUP;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("[%s:%s] set_com_audio err\n", MUIC_DEV_NAME, __func__);

	set_manual_sw(muic_data, MANSW_MANUAL); /* auto:0  manual:1 */

	return ret;
}

static int switch_to_gnd(struct sm5714_muic_data *muic_data)
{
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	ret = com_to_GND(muic_data);
	if (ret) {
		pr_err("[%s:%s] com->audio set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}
#endif

static int switch_to_ap_uart(struct sm5714_muic_data *muic_data, int new_dev)
{
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		ret = com_to_uart(muic_data);
#if defined(CONFIG_CP_UART_NOTI)
		send_uart_noti_to_modem(MODEM_CTRL_UART_AP);
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret) {
		pr_err("[%s:%s] com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_uart(struct sm5714_muic_data *muic_data, int new_dev)
{
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		ret = com_to_uart(muic_data);
#if defined(CONFIG_CP_UART_NOTI)
		send_uart_noti_to_modem(MODEM_CTRL_UART_CP);
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret) {
		pr_err("[%s:%s] com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int attach_jig_uart_path(struct sm5714_muic_data *muic_data, int new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	if (pdata->uart_path == MUIC_PATH_UART_AP)
		ret = switch_to_ap_uart(muic_data, new_dev);
	else
		ret = switch_to_cp_uart(muic_data, new_dev);

	return ret;
}

void sm5714_muic_set_bc12_control(int state)
{
	struct i2c_client *i2c = static_data->i2c;
	int reg_value1 = 0, reg_value2 = 0;

	pr_info("[%s:%s] state=%d\n", MUIC_DEV_NAME, __func__, state);
	if (static_data == NULL)
		return;

	reg_value1 = sm5714_i2c_read_byte(i2c, 0x32);
	reg_value1 = reg_value1 | 0x10;
	sm5714_i2c_write_byte(i2c, 0x32, reg_value1);

	reg_value2 = sm5714_i2c_read_byte(i2c, 0x8D);
	reg_value2 = reg_value2 & 0xFC;
	if (state == 1) /* 0:VBUS+DFP , 1:only VBUS*/
		reg_value2 = reg_value2 | 0x01;
	sm5714_i2c_write_byte(i2c, 0x8D, reg_value2);

	reg_value1 = reg_value1 & 0xEF;
	sm5714_i2c_write_byte(i2c, 0x32, reg_value1);

	pr_info("[%s:%s] reg_value1=0x%x, reg_value2=0x%x\n", MUIC_DEV_NAME,
			__func__, reg_value1, reg_value2);
}
EXPORT_SYMBOL(sm5714_muic_set_bc12_control);

#ifndef CONFIG_SEC_FACTORY
static void sm5714_muic_set_bc12(void *data, int enable)
{
	struct sm5714_muic_data *muic_data = data;
	struct i2c_client *i2c = muic_data->i2c;
	int reg_value = 0;

	reg_value = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CNTL);
	if (reg_value < 0) {
		pr_err("[%s:%s] failed, return\n", MUIC_DEV_NAME, __func__);
		return;
	}
	if (enable == 1) /* 1:bc12 enable , 0:bc12 disable*/
		reg_value = reg_value & 0xFD;
	else
		reg_value = reg_value | 0x02;
	sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_CNTL, reg_value);
	pr_info("[%s:%s] enable = %d, reg_value = 0x%x\n", MUIC_DEV_NAME,
			__func__, enable, reg_value);
}
#else
static void sm5714_muic_set_bc12(void *data, int enable)
{
	pr_info("[%s:%s] Do nothing\n", MUIC_DEV_NAME, __func__);

	return;
}
#endif

#if defined(CONFIG_MUIC_BCD_RESCAN)
static void sm5714_muic_BCD_rescan(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int reg_ctrl1 = 0, reg_ctrl2 = 0;

	pr_info("[%s:%s] BCD_RESCAN\n", MUIC_DEV_NAME, __func__);

	reg_ctrl1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CNTL);
	reg_ctrl2 = reg_ctrl1 | 0x01;
	sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_CNTL, reg_ctrl2);

	reg_ctrl2 = reg_ctrl2 & 0xFE;
	sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_CNTL, reg_ctrl2);

	pr_info("[%s:%s] CONTROL: reg_ctrl1=0x%x, reg_ctrl2=0x%x\n", MUIC_DEV_NAME,
			__func__, reg_ctrl1, reg_ctrl2);
}
#endif

int sm5714_muic_get_jig_status(void)
{
	struct i2c_client *i2c = static_data->i2c;
	int dev2 = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);
	if (static_data == NULL)
		return 0;

	dev2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE2);
	pr_info("[%s:%s]dev2 = 0x%x\n", MUIC_DEV_NAME, __func__, dev2);
	if ((dev2 & DEV_TYPE2_JIG_UART_OFF) || (dev2 & DEV_TYPE2_JIG_UART_ON))
		return 1;

	return 0;
}
EXPORT_SYMBOL(sm5714_muic_get_jig_status);

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
static int sm5714_muic_if_check_usb_killer(void *data)
{
	pr_info("[%s:%s] Do nothing\n", MUIC_DEV_NAME, __func__);

	return 0;
}

static void sm5714_muic_set_bypass(void *data, int enable)
{
	struct sm5714_muic_data *muic_data = data;

	pr_info("[%s:%s] enable : %d\n", MUIC_DEV_NAME, __func__, enable);

	/* handle detach event when bypass mode is enabled */
	if (enable && muic_data->attached_dev)
		sm5714_muic_handle_detach(muic_data, 0);

	/* disable bc12 when bypass mode */
	sm5714_muic_set_bc12((void *)muic_data, !enable);
}
#endif

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
struct muic_ops ops_muic = {
	.muic_check_usb_killer = sm5714_muic_if_check_usb_killer,
	.muic_set_bypass = sm5714_muic_set_bypass,
	.muic_set_bc12 = sm5714_muic_set_bc12,
};
#endif

static int sm5714_muic_get_adc(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int adc = 0;
	int dev2 = 0;

	dev2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE2);

	if (dev2 & 0x01)  {		/* JIG_USB_ON   : 301K*/
		adc = 0x19;
	} else if (dev2 & 0x02) {	/* JIG_USB_OFF  : 255K*/
		adc = 0x18;
	} else if (dev2 & 0x04) {	/* JIG_UART_ON  : 619K*/
		adc = 0x1D;
	} else if (dev2 & 0x08) {	/* JIG_UART_OFF : 523K*/
		adc = 0x1C;
	} else {
		adc = 0x1f;
	}
	pr_info("[%s:%s] dev2:0x%X, adc:0x%X\n",
		MUIC_DEV_NAME, __func__, dev2, adc);
	return adc;
}

static int sm5714_muic_get_factory_mode_rid(int new_dev)
{
	int ret = RID_OPEN;

	switch (new_dev) {
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = RID_301K;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		ret = RID_523K;
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		ret = RID_619K;
		break;
	default:
		break;
	}

	return ret;
}

static void sm5714_muic_set_factory_mode(struct sm5714_muic_data *muic_data,
		int new_dev, int intr)
{
	int rid = sm5714_muic_get_factory_mode_rid(new_dev);

	pr_info("[%s:%s] rid:%d , intr:%d\n",
		MUIC_DEV_NAME, __func__, rid, intr);

	if (rid == RID_OPEN)
		return;

	if (intr != MUIC_INTR_ATTACH)
		return;

#if defined(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_CHARGER_SM5714)
	if ((muic_data->pdic_info_data.pdic_evt_rid == RID_UNDEFINED)
			|| (muic_data->pdic_info_data.pdic_evt_rid == RID_OPEN))
		sm5714_charger_oper_en_factory_mode(DEV_TYPE_SM5714_MUIC,
							rid, 1);
#endif
}

static void sm5714_muic_handle_logically_detach(
		struct sm5714_muic_data *muic_data, int new_dev, int irq)
{
	int ret = 0;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_LO_TA_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC &&
				new_dev != ATTACHED_DEV_JIG_UART_OFF_VB_MUIC &&
				new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC &&
				new_dev != ATTACHED_DEV_JIG_UART_ON_VB_MUIC) {
			ret = com_to_open(muic_data);
		}
		break;

	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC &&
				new_dev != ATTACHED_DEV_JIG_UART_OFF_VB_MUIC &&
				new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC &&
				new_dev != ATTACHED_DEV_JIG_UART_ON_VB_MUIC) {
			ret = com_to_open(muic_data);
		}
		break;

	default:
		break;
	}

	if (ret)
		pr_err("[%s:%s] something wrong %d (ERR=%d)\n",
			MUIC_DEV_NAME, __func__, new_dev, ret);

	sm5714_muic_set_factory_mode(muic_data, muic_data->attached_dev,
				MUIC_INTR_DETACH);

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	if (!muic_data->suspended)
		muic_notifier_detach_attached_dev(muic_data->attached_dev);
	else
		muic_data->need_to_noti = true;
#endif /* CONFIG_MUIC_NOTIFIER */
}

static void sm5714_muic_handle_attach(struct sm5714_muic_data *muic_data,
			int new_dev, u8 vbvolt, int irq)
{
	int ret = 0;
	bool noti = (new_dev != muic_data->attached_dev) ? true : false;
#if defined(CONFIG_USB_HW_PARAM) && defined(CONFIG_MUIC_BCD_RESCAN)
	struct otg_notify *o_notify = get_otg_notify();
#endif /* CONFIG_USB_HW_PARAM */

	if (muic_data->is_water_detect) {
		pr_info("[%s:%s] skipped by water detected condition\n",
				MUIC_DEV_NAME, __func__);
		return;
	}

	pr_info("[%s:%s] attached_dev:%d, new_dev:%d, suspended:%d\n",
		MUIC_DEV_NAME, __func__, muic_data->attached_dev, new_dev,
		muic_data->suspended);

	if (noti)
		sm5714_muic_handle_logically_detach(muic_data, new_dev, irq);

	switch (new_dev) {
	case ATTACHED_DEV_OTG_MUIC:
#ifndef CONFIG_SEC_FACTORY
		sm5714_muic_set_bc12((void *)muic_data, 0);
#endif
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = com_to_usb(muic_data);
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
		muic_data->need_to_path_open = true;
#endif
		break;
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		if (get_com_sw(muic_data) == MANSW_UART)
			pr_info("[%s:%s] skip path setting\n", MUIC_DEV_NAME,
					__func__);
		else
			ret = com_to_usb(muic_data);
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
		muic_data->need_to_path_open = true;
#endif
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = com_to_usb(muic_data);
		break;
	case ATTACHED_DEV_TA_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_LO_TA_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = attach_jig_uart_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		ret = attach_jig_uart_path(muic_data, new_dev);
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		ret = com_to_open(muic_data);
		break;
	default:
		noti = false;
		pr_info("[%s:%s] unsupported dev=%d, vbus=%c\n",
			MUIC_DEV_NAME, __func__, new_dev, (vbvolt ? 'O' : 'X'));
		break;
	}

	if (ret)
		pr_err("[%s:%s] something wrong %d (ERR=%d)\n",
			MUIC_DEV_NAME, __func__, new_dev, ret);

	sm5714_muic_set_factory_mode(muic_data, new_dev, MUIC_INTR_ATTACH);

	pr_info("[%s:%s] done\n", MUIC_DEV_NAME, __func__);

	muic_data->attached_dev = new_dev;

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	if (noti) {
		if (!muic_data->suspended) {
			if (new_dev == ATTACHED_DEV_TA_MUIC)
				msleep(60);
			muic_notifier_attach_attached_dev(new_dev);
		} else {
			muic_data->need_to_noti = true;
			pr_info("[%s:%s] muic_data->need_to_noti = true\n",
					MUIC_DEV_NAME, __func__);
		}
	} else {
		pr_info("[%s:%s] attach Noti. for (%d) discarded.\n",
				MUIC_DEV_NAME, __func__, new_dev);
	}
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_USB_HW_PARAM) && defined(CONFIG_MUIC_BCD_RESCAN)
	if (o_notify &&
		muic_data->bc12_retry_count >= 1 &&
		new_dev != ATTACHED_DEV_TIMEOUT_OPEN_MUIC)
		inc_hw_param(o_notify, USB_MUIC_BC12_RETRY_SUCCESS_COUNT);
#endif /* CONFIG_USB_HW_PARAM */
}

static void sm5714_muic_handle_detach(struct sm5714_muic_data *muic_data,
		int irq)
{
	int ret = 0;
	bool noti = true;
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_attached_dev_t attached_dev = muic_data->attached_dev;
#endif

	pr_info("[%s:%s] attached_dev:%d\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

#ifndef CONFIG_SEC_FACTORY
	sm5714_muic_set_bc12((void *)muic_data, 1);
#endif

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
		if (muic_data->pdic_info_data.pdic_evt_attached ==
				MUIC_PDIC_NOTI_DETACH) {
			ret = com_to_open(muic_data);
			muic_data->need_to_path_open = false;
		}
#else
		ret = com_to_open(muic_data);
#endif
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_UNKNOWN_MUIC:
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
		if (muic_data->need_to_path_open) {
			ret = com_to_open(muic_data);
			muic_data->need_to_path_open = false;
		}
#endif
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC:
		sm5714_set_afc_ctrl_reg(muic_data, AFCCTRL_ENAFC, 0);
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_LO_TA_MUIC:
		ret = com_to_open(muic_data);
		break;
	default:
		noti = false;
		pr_info("[%s:%s] invalid type(%d)\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);
		break;
	}
	if (ret)
		pr_err("[%s:%s] something wrong %d (ERR=%d)\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev, ret);

	if ((muic_data->attached_dev == ATTACHED_DEV_JIG_USB_OFF_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_JIG_USB_ON_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_JIG_UART_ON_VB_MUIC) ||
		(muic_data->attached_dev == ATTACHED_DEV_JIG_UART_ON_MUIC)) {
		cancel_delayed_work(&muic_data->delayed_523Kto619K_work);
		muic_data->delayed_523Kto619K_state = 0;
	}

	sm5714_muic_set_factory_mode(muic_data, muic_data->attached_dev,
			MUIC_INTR_DETACH);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
	muic_data->vbus_changed_9to5 = 0;
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	if (noti) {
		if (!muic_data->suspended) {
			muic_notifier_detach_attached_dev(attached_dev);
		} else {
			muic_data->need_to_noti = true;
			pr_info("[%s:%s] muic_data->need_to_noti = true\n",
				MUIC_DEV_NAME, __func__);
		}
	} else {
		pr_info("[%s:%s] detach Noti. for (%d) discarded.\n",
			MUIC_DEV_NAME, __func__, attached_dev);
	}
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_MUIC_BCD_RESCAN)
	muic_data->bc12_retry_count = 0;
#endif
	muic_data->hv_voltage = 0;
	muic_afc_request_cause_clear();
}

static void sm5714_muic_detect_dev(struct sm5714_muic_data *muic_data, int irq)
{
	struct i2c_client *i2c = muic_data->i2c;
	int new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	/* struct otg_notify *o_notify = get_otg_notify(); */
	int intr = MUIC_INTR_DETACH;
	int dev1 = 0, dev2 = 0, ctrl = 0, manualsw = 0, afcctrl = 0, vbvolt = 0;

	dev1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE1);
	dev2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE2);
	ctrl = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CNTL);
	manualsw = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_MANUAL_SW);
	afcctrl = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCCNTL);
	vbvolt = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS);
	vbvolt = (vbvolt&0x04)>>2;

	pr_info("[%s:%s] dev1:0x%02x, dev2:0x%02x, ctrl:0x%02x, ma:0x%02x, afcctrl:0x%02x, vbvolt:0x%02x\n",
			MUIC_DEV_NAME, __func__, dev1, dev2, ctrl,
			manualsw, afcctrl, vbvolt);

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	if (muic_data->pdata->opmode == OPMODE_PDIC) {
		switch (muic_data->pdic_info_data.pdic_evt_rid) {
		case RID_000K:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_OTG_MUIC;
			pr_info("[%s:%s] USB_OTG\n", MUIC_DEV_NAME, __func__);
			break;
		case RID_255K:
#if defined(CONFIG_SEC_FACTORY)
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
			pr_info("[%s:%s] JIG_USB_OFF(255K)\n", MUIC_DEV_NAME,
					__func__);
#else
			/*
			 * Don't set device type
			 * because 255K is used as Bypass mode
			 */
			dev1 = dev2 = 0;
			pr_info("[%s:%s] Bypass mode(255K)\n", MUIC_DEV_NAME,
					__func__);
#endif
			break;
		case RID_301K:
			if (!(vbvolt))
				break;
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
			pr_info("[%s:%s] JIG_USB_ON(301K)\n", MUIC_DEV_NAME,
					__func__);
			break;
		case RID_523K:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
			pr_info("[%s:%s] JIG_UART_OFF(523K)\n", MUIC_DEV_NAME,
					__func__);
			break;
		case RID_619K:
			intr = MUIC_INTR_ATTACH;
			if (vbvolt) {
				new_dev = ATTACHED_DEV_JIG_UART_ON_VB_MUIC;
				pr_info("[%s:%s] JIG_UART_ON(619K) + VBUS\n",
						MUIC_DEV_NAME, __func__);

				pr_info("[%s:%s] delayed_523Kto619K_state(%d), attached_dev(%d), new_dev(%d)\n",
					MUIC_DEV_NAME, __func__, muic_data->delayed_523Kto619K_state,
					muic_data->attached_dev, new_dev);

				if ((muic_data->delayed_523Kto619K_state == 0) &&
					(muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) &&
					(new_dev == ATTACHED_DEV_JIG_UART_ON_VB_MUIC)) {
					cancel_delayed_work(&muic_data->delayed_523Kto619K_work);
					schedule_delayed_work(&muic_data->delayed_523Kto619K_work,
							msecs_to_jiffies(1500));
					pr_info("[%s:%s] sm5714_muic_delayed_523Kto619K_work return\n",
							MUIC_DEV_NAME, __func__);
					muic_data->delayed_523Kto619K_state = 1;

					return;
				}
			} else {
				new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
				pr_info("[%s:%s] JIG_UART_ON(619K)\n",
						MUIC_DEV_NAME, __func__);
			}
			break;
		default:
			break;
		}

		if (muic_data->pdic_info_data.pdic_evt_rprd) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_OTG_MUIC;
			pr_info("[%s:%s] USB_OTG\n", MUIC_DEV_NAME, __func__);
		} else if (muic_data->pdic_info_data.pdic_evt_dcdcnt) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
			pr_info("[%s:%s] CC INCOMPLETE INSERTION\n",
					MUIC_DEV_NAME, __func__);
		}

		if (intr == MUIC_INTR_ATTACH) {
			/* do not check device type register */
			dev1 = dev2 = 0;
		}
	}
#endif

	muic_data->dev1 = dev1;
	muic_data->dev2 = dev2;

	if (dev1 & DEV_TYPE1_LO_TA) {
		if (!vbvolt) {
			pr_info("[%s:%s] DEV_TYPE1_LO_TA + NO VBUS\n",
					MUIC_DEV_NAME, __func__);
			return;
		}
		intr = MUIC_INTR_ATTACH;
#if IS_ENABLED(CONFIG_MUIC_LO_TA_LOW_CURRENT)
		new_dev = ATTACHED_DEV_LO_TA_MUIC;
#else
		new_dev = ATTACHED_DEV_UNOFFICIAL_TA_MUIC;
#endif
		pr_info("[%s:%s] LO_TA\n", MUIC_DEV_NAME, __func__);
	} else if (dev1 & DEV_TYPE1_U200) {
		if ((irq == SM5714_MUIC_IRQ_WORK) || (irq == SM5714_MUIC_IRQ_PROBE)) {
			if (!vbvolt) {
				pr_info("[%s:%s] DEV_TYPE1_U200 + NO VBUS\n",
						MUIC_DEV_NAME, __func__);
				return;
			}
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_UNOFFICIAL_TA_MUIC;
			pr_info("[%s:%s] U200\n", MUIC_DEV_NAME, __func__);

		} else {
			schedule_delayed_work(&muic_data->muic_U200_work,
				msecs_to_jiffies(100)); /* 100ms */
			pr_info("[%s:%s] muic_U200_work start\n",
				MUIC_DEV_NAME, __func__);
			return;
		}
	} else if (dev1 & DEV_TYPE1_CDP) {
		if (!vbvolt) {
			pr_info("[%s:%s] DEV_TYPE1_CDP + NO VBUS\n",
					MUIC_DEV_NAME, __func__);
			return;
		}
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("[%s:%s] CDP\n", MUIC_DEV_NAME, __func__);
	} else if ((dev1 & DEV_TYPE1_DCP) ||
				(dev1 & DEV_TYPE1_AFC_DCP) ||
				(dev1 & DEV_TYPE1_QC20_DCP)) {
		if (!vbvolt) {
			pr_info("[%s:%s] DEV_TYPE1_DCP + NO VBUS\n",
					MUIC_DEV_NAME, __func__);
			return;
		}
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("[%s:%s] DCP\n", MUIC_DEV_NAME, __func__);
	} else if (dev1 & DEV_TYPE1_SDP) {
		if (!vbvolt) {
			pr_info("[%s:%s] DEV_TYPE1_SDP(USB) + NO VBUS\n",
					MUIC_DEV_NAME, __func__);
			return;
		}
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("[%s:%s] USB(SDP)\n", MUIC_DEV_NAME, __func__);
	} else if (dev1 & DEV_TYPE1_DCD_OUT_SDP) {
		if (!vbvolt) {
			pr_info("[%s:%s] DEV_TYPE1_DCD_OUT_SDP + NO VBUS\n",
					MUIC_DEV_NAME, __func__);
			return;
		}

#if defined(CONFIG_MUIC_BCD_RESCAN)
		if (muic_data->pdic_afc_state == SM5714_MUIC_AFC_NORMAL && muic_data->bc12_retry_count < muic_data->bc1p2_retry_count_max) {
			muic_data->bc12_retry_count++;
			pr_info("[%s:%s] DCD_OUT_SDP (bc12_retry_count=%d)\n",
					MUIC_DEV_NAME, __func__, muic_data->bc12_retry_count);
			msleep(50);
			sm5714_muic_BCD_rescan(muic_data);
			return;
		}
#endif

		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
		pr_info("[%s:%s] DCD_OUT_SDP\n", MUIC_DEV_NAME, __func__);
	}

	if (dev2 & DEV_TYPE2_JIG_UART_OFF) {
		intr = MUIC_INTR_ATTACH;

		if (vbvolt) {
			new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
			pr_info("[%s:%s] JIG_UART_OFF(523K) + VBUS\n",
					MUIC_DEV_NAME, __func__);
		} else {
			new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
			pr_info("[%s:%s] JIG_UART_OFF(523K)\n",
					MUIC_DEV_NAME, __func__);
		}
	} else if (dev2 & DEV_TYPE2_JIG_UART_ON) {
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		pr_info("[%s:%s] JIG_UART_ON(619K)\n",
				MUIC_DEV_NAME, __func__);
	} else if (dev2 & DEV_TYPE2_JIG_USB_OFF) {
		if (vbvolt) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
			pr_info("[%s:%s] JIG_USB_OFF(255K)\n",
				MUIC_DEV_NAME, __func__);
		}
	} else if (dev2 & DEV_TYPE2_JIG_USB_ON) {
		if (vbvolt) {
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
			pr_info("[%s:%s] JIG_USB_ON(301K)\n",
				MUIC_DEV_NAME, __func__);
		}
	} else if (dev2 & DEV_TYPE2_DEBUG_JTAG) {
		pr_info("[%s:%s] DEBUG_JTAG\n", MUIC_DEV_NAME, __func__);
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_NONE_MUIC;
		com_to_JTAG(muic_data);
	}

	if (intr == MUIC_INTR_ATTACH)
		sm5714_muic_handle_attach(muic_data, new_dev, vbvolt, irq);
	else
		sm5714_muic_handle_detach(muic_data, irq);
}

static int sm5714_muic_reg_init(struct sm5714_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int intmask1 = 0, intmask2 = 0, cntl = 0, manualsw = 0;
	int dev1 = 0, dev2 = 0, afccntl = 0, afctxd = 0;
	int afcstatus = 0, vbus_voltage = 0;
	int cfg1 = 0, cfg2 = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_CNTL, 0x24);
	sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_AFCCNTL, 0x00);

	intmask1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_INTMASK1);
	intmask2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_INTMASK2);
	cntl = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CNTL);
	manualsw = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_MANUAL_SW);

	dev1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE1);
	dev2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE2);
	afccntl = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCCNTL);
	afctxd = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCTXD);

	afcstatus = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCSTATUS);
	vbus_voltage = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS_VOLTAGE);

	cfg1 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CFG1);
	cfg2 = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CFG2);

	/* set dcd timer out to 0.73s */
	cntl &= ~CTRL_DCDTIMER_MASK;
	cntl |= (CTRL_DCDTIMER_MASK & 0x08);
	sm5714_i2c_write_byte(i2c, SM5714_MUIC_REG_CNTL, cntl);

	pr_info("[%s:%s] intmask1:0x%x, intmask2:0x%x, cntl:0x%x, mansw:0x%x\n",
		MUIC_DEV_NAME, __func__, intmask1, intmask2, cntl, manualsw);
	pr_info("[%s:%s] dev1:0x%x, dev2:0x%x, afccntl:0x%x, afctxd:0x%x\n",
		MUIC_DEV_NAME, __func__, dev1, dev2, afccntl, afctxd);
	pr_info("[%s:%s] afcstatus:0x%x, vbus_voltage:0x%x\n",
		MUIC_DEV_NAME, __func__, afcstatus, vbus_voltage);

	pr_info("[%s:%s] cfg1:0x%x, cfg2=0x%x\n",
		MUIC_DEV_NAME, __func__, cfg1, cfg2);

	return ret;
}

static irqreturn_t sm5714_muic_irq_thread(int irq, void *data)
{
	struct sm5714_muic_data *muic_data = data;
	int irq_num = irq - muic_data->mfd_pdata->irq_base;

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->wake_lock);

	if (irq != (SM5714_MUIC_IRQ_PROBE)) {
		pr_info("[%s:%s] irq_gpio(%d), irq (%d), irq_num(%d:%s)\n",
				MUIC_DEV_NAME, __func__,
				muic_data->mfd_pdata->irq_base, irq, irq_num,
				SM5714_MUIC_INT_NAME[irq_num]);
	} else {
		pr_info("[%s:%s] irq_gpio(%d), irq (%d), irq_num(%d)\n",
				MUIC_DEV_NAME, __func__,
				muic_data->mfd_pdata->irq_base, irq, irq_num);
	}

	if (irq_num == SM5714_MUIC_IRQ_INT1_VBUS_RID_DETACH) {
		pr_info("%s: afc_retry_work(INT1_DETACH) cancel\n", __func__);
		cancel_delayed_work(&muic_data->afc_retry_work);
		cancel_delayed_work(&muic_data->afc_torch_work);
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
		cancel_delayed_work(&muic_data->pdic_afc_work);
#endif
		cancel_delayed_work(&muic_data->delayed_523Kto619K_work);
		muic_data->delayed_523Kto619K_state = 0;
		cancel_delayed_work(&muic_data->muic_U200_work);
	}

	if (irq_num == SM5714_MUIC_IRQ_INT2_AFC_ERROR) {
		sm5714_afc_error(muic_data);
		goto EOH;
	}

	if (irq_num == SM5714_MUIC_IRQ_INT2_AFC_STA_CHG) {
		sm5714_afc_sta_chg(muic_data);
		goto EOH;
	}

	if (irq_num == SM5714_MUIC_IRQ_INT2_MULTI_BYTE) {
		sm5714_afc_multi_byte(muic_data);
		goto EOH;
	}

	if (irq_num == SM5714_MUIC_IRQ_INT2_AFC_ACCEPTED) {
		sm5714_afc_ta_accept(muic_data);
		goto EOH;
	}

	if (irq_num == SM5714_MUIC_IRQ_INT2_AFC_TA_ATTACHED) {
		sm5714_afc_ta_attach(muic_data);
		goto EOH;
	}

	if ((irq == (SM5714_MUIC_IRQ_PROBE)) ||
		(irq_num == SM5714_MUIC_IRQ_INT1_DPDM_OVP) ||
		(irq_num == SM5714_MUIC_IRQ_INT1_VBUS_RID_DETACH) ||
		(irq_num == SM5714_MUIC_IRQ_INT1_RID_DETECT) ||
		(irq_num == SM5714_MUIC_IRQ_INT1_CHGTYPE) ||
		(irq_num == SM5714_MUIC_IRQ_INT1_DCDTIMEOUT)) {

		sm5714_muic_detect_dev(muic_data, irq);
	}

EOH:
	__pm_relax(muic_data->wake_lock);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("[%s:%s] done\n", MUIC_DEV_NAME, __func__);
	return IRQ_HANDLED;
}

static void sm5714_muic_handle_event(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data = container_of(work,
			struct sm5714_muic_data, muic_event_work);

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->wake_lock);
	sm5714_muic_detect_dev(muic_data, SM5714_MUIC_IRQ_WORK);
	__pm_relax(muic_data->wake_lock);
	mutex_unlock(&muic_data->muic_mutex);
}

static void sm5714_muic_U200_delayed_noti(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data = static_data;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->wake_lock);
	sm5714_muic_detect_dev(muic_data, SM5714_MUIC_IRQ_WORK);
	__pm_relax(muic_data->wake_lock);
	mutex_unlock(&muic_data->muic_mutex);
}

static void sm5714_muic_delayed_523Kto619K_work(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data = static_data;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->wake_lock);
	sm5714_muic_detect_dev(muic_data, SM5714_MUIC_IRQ_WORK);
	muic_data->delayed_523Kto619K_state = 0;
	__pm_relax(muic_data->wake_lock);
	mutex_unlock(&muic_data->muic_mutex);
}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
static int sm5714_muic_handle_vbus_notification(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct sm5714_muic_data *muic_data = container_of(nb,
			struct sm5714_muic_data, vbus_nb);
	int vbus_type = *(int *)data;

	muic_data->vbus_state = vbus_type;

	if (muic_data->is_water_detect) {
		pr_info("[%s:%s] vbus(%d) water(%d)\n", MUIC_DEV_NAME, __func__,
				vbus_type, muic_data->is_water_detect);

		switch (vbus_type) {
		case STATUS_VBUS_HIGH:
			muic_set_hiccup_mode(1);
			break;
		case STATUS_VBUS_LOW:
			break;
		default:
			break;
		}
	}

	return 0;
}
#endif /* CONFIG_VBUS_NOTIFIER */


#if defined(CONFIG_HICCUP_CHARGER)
static int sm5714_muic_pdic_set_hiccup_mode(int val)
{
	struct sm5714_muic_data *muic_data = static_data;

	if (static_data == NULL)
		return -ENODEV;

	if (is_lpcharge_pdic_param())
		return 0;
#if IS_ENABLED(CONFIG_HICCUP_CC_DISABLE)
	if (muic_data->shut_down) {
		pr_info("[%s:%s] do not set after shutdown.\n", MUIC_DEV_NAME, __func__);
		return 0;
	}
#endif
	pr_info("[%s:%s] val = %d\n", MUIC_DEV_NAME, __func__, val);

	if (val == 1) { /* Hiccup mode on */
		muic_notifier_attach_attached_dev(ATTACHED_DEV_HICCUP_MUIC);
		switch_to_gnd(muic_data);
	} else { /* Hiccup mode off */
		com_to_open(muic_data);
	}

	return 0;
}
#endif

static int sm5714_muic_hv_charger_init(void)
{
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct sm5714_muic_data *muic_data = static_data;

	/* enable hiccup mode after charger init*/
	if (muic_data->is_water_detect &&
			muic_data->vbus_state == STATUS_VBUS_HIGH)
		muic_set_hiccup_mode(1);
#endif /* CONFIG_VBUS_NOTIFIER */

	return sm5714_muic_charger_init();
}

static void sm5714_muic_debug_reg_log(struct work_struct *work)
{
	struct sm5714_muic_data *muic_data = container_of(work,
			struct sm5714_muic_data, muic_debug_work.work);
	struct i2c_client *i2c = muic_data->i2c;
	u8 data[18] = {0, };

	data[0] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_INTMASK1);
	data[1] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_INTMASK2);
	data[2] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CNTL);
	data[3] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_MANUAL_SW);
	data[4] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE1);
	data[5] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_DEVICETYPE2);

	data[6] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCCNTL);
	data[7] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCTXD);
	data[8] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFCSTATUS);
	data[9] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS_VOLTAGE);
	data[10] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFC_STATE);

	data[11] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CFG1);
	data[12] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_CFG2);
	data[13] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_USBKS);
	data[14] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_VBUS);
	data[15] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_BCD_STATE);
	data[16] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_AFC_STATE);
	data[17] = sm5714_i2c_read_byte(i2c, SM5714_MUIC_REG_OTP_IF_STS);

	pr_info("[%s:%s]IM[0x%02x 0x%02x] CNTL:0x%02x MAN_SW:0x%02x DEV_TYPE[0x%02x 0x%02x] attached_dev:%d\n",
			MUIC_DEV_NAME, __func__, data[0], data[1], data[2],
			data[3], data[4], data[5], muic_data->attached_dev);

	pr_info("[%s:%s]AFC_CNTL:0x%02x AFC_TXD:0x%02x AFC_STATUS:0x%02x VBUS_VOL:%d AFC_STA:0x%02x",
			MUIC_DEV_NAME, __func__, data[6], data[7],
			data[8], data[9], data[10]);

	pr_info("[%s:%s]CFG1:0x%02x CFG2:0x%02x USBKS:0x%02x VBUS:0x%02x BCD_STATE:0x%02x AFC_STATE:0x%02x OTP_IF_STS:0x%02x\n",
			MUIC_DEV_NAME, __func__, data[11], data[12], data[13],
			data[14], data[15], data[16], data[17]);

	if (!muic_data->suspended)
		schedule_delayed_work(&muic_data->muic_debug_work,
				msecs_to_jiffies(60000));
}

static int sm5714_init_rev_info(struct sm5714_muic_data *muic_data)
{
	int dev_id = 0;
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	dev_id = sm5714_i2c_read_byte(muic_data->i2c, SM5714_MUIC_REG_DeviceID);
	if (dev_id < 0) {
		pr_err("[%s:%s] dev_id(%d)\n", MUIC_DEV_NAME, __func__, dev_id);
		ret = -ENODEV;
	} else {
		muic_data->muic_vendor = (dev_id & 0x07);
		muic_data->muic_version = 0x00;
		pr_info("[%s:%s] vendor=0x%x, ver=0x%x, dev_id=0x%x\n",
			MUIC_DEV_NAME, __func__, muic_data->muic_vendor,
			muic_data->muic_version, dev_id);
	}
	return ret;
}

#define REQUEST_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, sm5714_muic_irq_thread,	\
				0, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("[%s:%s] Failed to request IRQ #%d: %d\n",	\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

static int sm5714_muic_irq_init(struct sm5714_muic_data *muic_data)
{
	int ret = 0;

	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		/* request MUIC IRQ */
		muic_data->irqs.irq_dpdm_ovp =
			irq_base + SM5714_MUIC_IRQ_INT1_DPDM_OVP;
		REQUEST_IRQ(muic_data->irqs.irq_dpdm_ovp, muic_data,
				"muic-dpdm_ovp");

		muic_data->irqs.irq_vbus_rid_detach =
			irq_base + SM5714_MUIC_IRQ_INT1_VBUS_RID_DETACH;
		REQUEST_IRQ(muic_data->irqs.irq_vbus_rid_detach, muic_data,
				"muic-vbus_rid_detach");

		muic_data->irqs.irq_rid_detect =
			irq_base + SM5714_MUIC_IRQ_INT1_RID_DETECT;
		REQUEST_IRQ(muic_data->irqs.irq_rid_detect, muic_data,
				"muic-rid_detect");

		muic_data->irqs.irq_chgtype_attach =
			irq_base + SM5714_MUIC_IRQ_INT1_CHGTYPE;
		REQUEST_IRQ(muic_data->irqs.irq_chgtype_attach, muic_data,
				"muic-chgtype_attach");

		muic_data->irqs.irq_dectimeout =
			irq_base + SM5714_MUIC_IRQ_INT1_DCDTIMEOUT;
		REQUEST_IRQ(muic_data->irqs.irq_dectimeout, muic_data,
				"muic-dectimeout");

		muic_data->irqs.irq_afc_error =
			irq_base + SM5714_MUIC_IRQ_INT2_AFC_ERROR;
		REQUEST_IRQ(muic_data->irqs.irq_afc_error, muic_data,
				"muic-afc_error");

		muic_data->irqs.irq_afc_sta_chg =
			irq_base + SM5714_MUIC_IRQ_INT2_AFC_STA_CHG;
		REQUEST_IRQ(muic_data->irqs.irq_afc_sta_chg, muic_data,
				"muic-afc_sta_chg");

		muic_data->irqs.irq_multi_byte =
			irq_base + SM5714_MUIC_IRQ_INT2_MULTI_BYTE;
		REQUEST_IRQ(muic_data->irqs.irq_multi_byte, muic_data,
				"muic-multi_byte");

		muic_data->irqs.irq_afc_accepted =
			irq_base + SM5714_MUIC_IRQ_INT2_AFC_ACCEPTED;
		REQUEST_IRQ(muic_data->irqs.irq_afc_accepted, muic_data,
				"muic-afc_accepted");

		muic_data->irqs.irq_afc_ta_attached =
			irq_base + SM5714_MUIC_IRQ_INT2_AFC_TA_ATTACHED;
		REQUEST_IRQ(muic_data->irqs.irq_afc_ta_attached, muic_data,
				"muic-afc_ta_attached");

	}

	pr_info("[%s:%s] muic-dpdm_ovp(%d), muic-vbus_rid_detach(%d), muic-rid_detect(%d)",
		MUIC_DEV_NAME, __func__, muic_data->irqs.irq_dpdm_ovp,
		muic_data->irqs.irq_vbus_rid_detach,
		muic_data->irqs.irq_rid_detect);

	pr_info("[%s:%s] muic-chgtype_attach(%d), muic-dectimeout(%d), muic-afc_error(%d), muic-afc_sta_chg(%d)\n",
		MUIC_DEV_NAME, __func__, muic_data->irqs.irq_chgtype_attach,
		muic_data->irqs.irq_dectimeout, muic_data->irqs.irq_afc_error,
		muic_data->irqs.irq_afc_sta_chg);

	pr_info("[%s:%s] muic-multi_byte(%d), muic-afc_accepted(%d), muic-afc_ta_attached(%d)\n",
		MUIC_DEV_NAME, __func__, muic_data->irqs.irq_multi_byte,
		muic_data->irqs.irq_afc_accepted,
		muic_data->irqs.irq_afc_ta_attached);

	return ret;
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("[%s:%s] IRQ(%d):%s free done\n", MUIC_DEV_NAME,\
				__func__, _irq, _name);			\
	}								\
} while (0)

static void sm5714_muic_free_irqs(struct sm5714_muic_data *muic_data)
{
	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	/* free MUIC IRQ */
	FREE_IRQ(muic_data->irqs.irq_dpdm_ovp, muic_data, "muic-dpdm_ovp");
	FREE_IRQ(muic_data->irqs.irq_vbus_rid_detach, muic_data,
			"muic-vbus_rid_detach");
	FREE_IRQ(muic_data->irqs.irq_rid_detect, muic_data, "muic-rid_detect");
	FREE_IRQ(muic_data->irqs.irq_chgtype_attach, muic_data,
			"muic-chgtype_attach");
	FREE_IRQ(muic_data->irqs.irq_dectimeout, muic_data, "muic-dectimeout");

	FREE_IRQ(muic_data->irqs.irq_afc_error, muic_data, "muic-afc_error");
	FREE_IRQ(muic_data->irqs.irq_afc_sta_chg, muic_data,
			"muic-afc_sta_chg");
	FREE_IRQ(muic_data->irqs.irq_multi_byte, muic_data, "muic-multi_byte");
	FREE_IRQ(muic_data->irqs.irq_afc_accepted, muic_data,
			"muic-afc_accepted");
	FREE_IRQ(muic_data->irqs.irq_afc_ta_attached, muic_data,
			"muic-afc_ta_attached");

}

#if defined(CONFIG_MUIC_BCD_RESCAN)
static void sm5714_set_bc1p2_retry_count(struct sm5714_muic_data *muic_data)
{
	struct device_node *np = NULL;
	int count;

	np = of_find_compatible_node(NULL, NULL, "siliconmitus,sm5714mfd");

	if (np && !of_property_read_u32(np, "sm5714,bc1p2_retry_count", &count))
		muic_data->bc1p2_retry_count_max = count;
	else
		muic_data->bc1p2_retry_count_max = 1; /* default retry count */

	pr_info("%s:%s BC1p2 Retry count: %d\n", MUIC_DEV_NAME,
				__func__, muic_data->bc1p2_retry_count_max);
}
#endif

#if defined(CONFIG_OF)
static int of_sm5714_muic_dt(struct device *dev,
		struct sm5714_muic_data *muic_data)
{
	struct device_node *np, *np_muic;
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	np = dev->parent->of_node;
	if (!np) {
		pr_err("[%s:%s] could not find np\n", MUIC_DEV_NAME, __func__);
		return -ENODEV;
	}

	np_muic = of_find_node_by_name(np, "muic");
	if (!np_muic) {
		pr_err("[%s:%s] could not find muic sub-node np_muic\n",
				MUIC_DEV_NAME, __func__);
		return -EINVAL;
	}

	return ret;
}
#endif /* CONFIG_OF */

/* if need to set sm5714 pdata */
static const struct of_device_id sm5714_muic_match_table[] = {
	{ .compatible = "sm5714-muic",},
	{},
};

static int sm5714_muic_probe(struct platform_device *pdev)
{
	struct sm5714_dev *sm5714 = dev_get_drvdata(pdev->dev.parent);
	struct sm5714_platform_data *mfd_pdata = dev_get_platdata(sm5714->dev);
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	struct muic_dev *muic_d;
#endif
	struct sm5714_muic_data *muic_data;
	int ret = 0;

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

	muic_data = kzalloc(sizeof(struct sm5714_muic_data), GFP_KERNEL);
	if (!muic_data) {
		ret = -ENOMEM;
		goto err_return;
	}

	if (!mfd_pdata) {
		pr_err("[%s:%s] failed to get sm5714 mfd platform data\n",
				MUIC_DEV_NAME, __func__);
		ret = -ENOMEM;
		goto err_kfree1;
	}

	/* save platfom data for gpio control functions */
	static_data = muic_data;
	muic_data->pdata = &muic_pdata;
	muic_data->muic_data = muic_data;

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	muic_d = kzalloc(sizeof(struct muic_dev), GFP_KERNEL);
	if (!muic_d) {
		ret = -ENOMEM;
		goto err_kfree1;
	}
	muic_d->ops = &ops_muic;
	muic_d->data = (void *)muic_data;
	muic_data->man = register_muic(muic_d);
#endif

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	muic_data->pdata->opmode = get_pdic_info() & 0xF;
	muic_data->pdic_afc_state = SM5714_MUIC_AFC_NORMAL;
	muic_data->pdic_afc_state_count = 0;
	muic_data->need_to_path_open = false;
#endif

	muic_data->sm5714_dev = sm5714;
	muic_data->dev = &pdev->dev;
	muic_data->i2c = sm5714->muic;
	muic_data->mfd_pdata = mfd_pdata;

	muic_data->is_water_detect = false;
	muic_data->afc_irq_disabled = false;
	muic_data->vbus_changed_9to5 = 0;
	muic_data->fled_torch_enable = false;
	muic_data->fled_flash_enable = false;
	muic_data->hv_voltage = 0;
	muic_data->is_pdic_ready = false;

#if defined(CONFIG_HICCUP_CHARGER)
	muic_data->is_hiccup_mode = false;
	muic_data->pdata->muic_set_hiccup_mode_cb =
				sm5714_muic_pdic_set_hiccup_mode;
#endif

#if defined(CONFIG_MUIC_BCD_RESCAN)
	muic_data->bc12_retry_count = 0;
	sm5714_set_bc1p2_retry_count(muic_data);
#endif

#if defined(CONFIG_OF)
	ret = of_sm5714_muic_dt(&pdev->dev, muic_data);
	if (ret < 0)
		pr_err("[%s:%s] no muic dt! ret[%d]\n",
				MUIC_DEV_NAME, __func__, ret);
#endif /* CONFIG_OF */


	muic_data->is_factory_start = false;
	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;

	platform_set_drvdata(pdev, muic_data);

	mutex_init(&muic_data->muic_mutex);
	mutex_init(&muic_data->afc_mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	muic_data->wake_lock = wakeup_source_create("muic_wake");
	if (muic_data->wake_lock)
		wakeup_source_add(muic_data->wake_lock);
#else
	muic_data->wake_lock = wakeup_source_register(NULL, "muic_wake");
#endif

#if IS_MODULE(CONFIG_MUIC_NOTIFIER)
		ret = muic_data->pdata->init_gpio_cb(get_switch_sel());
#else
	if (muic_data->pdata->init_gpio_cb)
		ret = muic_data->pdata->init_gpio_cb();
#endif
	if (ret) {
		pr_err("[%s:%s] failed to init gpio(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
		goto fail_init_gpio;
	}

	/* create sysfs group */
	if (switch_device) {
		ret = sysfs_create_group(&switch_device->kobj, &sm5714_muic_group);
		if (ret) {
			pr_err("[%s:%s] failed to create sysfs\n",
					MUIC_DEV_NAME, __func__);
			goto fail;
		}
		dev_set_drvdata(switch_device, muic_data);
	}

	ret = sm5714_init_rev_info(muic_data);
	if (ret) {
		pr_err("[%s:%s] failed to init muic(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
		goto fail;
	}

	ret = sm5714_muic_reg_init(muic_data);
	if (ret) {
		pr_err("[%s:%s] failed to init muic(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
		goto fail;
	}

	sm5714_hv_muic_initialize(muic_data);


	muic_data->is_rustproof = muic_data->pdata->rustproof_on;
	if (muic_data->is_rustproof) {
		pr_err("[%s:%s] rustproof is enabled\n",
				MUIC_DEV_NAME, __func__);
		com_to_open(muic_data);
	}


	if (get_afc_mode() == CH_MODE_AFC_DISABLE_VAL) {
		pr_info("[%s:%s] AFC mode disabled\n", MUIC_DEV_NAME, __func__);
		muic_data->pdata->afc_disable = true;
	} else {
		pr_info("[%s:%s] AFC mode enabled\n", MUIC_DEV_NAME, __func__);
		muic_data->pdata->afc_disable = false;
	}

	muic_data->pdata->muic_afc_set_voltage_cb = sm5714_muic_afc_set_voltage;
	muic_data->pdata->muic_hv_charger_init_cb = sm5714_muic_hv_charger_init;

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	/* init works */
	INIT_WORK(&(muic_data->muic_event_work), sm5714_muic_handle_event);
	INIT_DELAYED_WORK(&muic_data->muic_debug_work,
						sm5714_muic_debug_reg_log);
	schedule_delayed_work(&muic_data->muic_debug_work,
						msecs_to_jiffies(10000));

	INIT_DELAYED_WORK(&muic_data->delayed_523Kto619K_work, sm5714_muic_delayed_523Kto619K_work);
	muic_data->delayed_523Kto619K_state = 0;

	INIT_DELAYED_WORK(&muic_data->muic_U200_work,
						sm5714_muic_U200_delayed_noti);

	/* initial cable detection */
	sm5714_muic_irq_thread(SM5714_MUIC_IRQ_PROBE, muic_data);

	ret = sm5714_muic_irq_init(muic_data);
	if (ret) {
		pr_err("[%s:%s] failed to init irq(%d)\n",
				MUIC_DEV_NAME, __func__, ret);
		goto fail_init_irq;
	}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&muic_data->vbus_nb,
			sm5714_muic_handle_vbus_notification,
			VBUS_NOTIFY_DEV_MUIC);
	muic_data->vbus_state = STATUS_VBUS_UNKNOWN;
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	if (muic_data->pdata->opmode & OPMODE_PDIC)
		sm5714_muic_register_pdic_notifier(muic_data);
	else
		pr_info("[%s:%s] OPMODE_MUIC, PDIC NOTIFIER is not used.\n",
				MUIC_DEV_NAME, __func__);
#endif

	muic_data->shut_down = 0;

	return 0;

fail_init_irq:
	sm5714_muic_free_irqs(muic_data);
fail:
	if (switch_device)
		sysfs_remove_group(&switch_device->kobj, &sm5714_muic_group);
	mutex_destroy(&muic_data->muic_mutex);
	mutex_destroy(&muic_data->afc_mutex);

fail_init_gpio:
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	kfree(muic_d);
#endif
err_kfree1:
	kfree(muic_data);
err_return:
	return ret;
}

static int sm5714_muic_remove(struct platform_device *pdev)
{
	struct sm5714_muic_data *muic_data = platform_get_drvdata(pdev);

	if (switch_device)
		sysfs_remove_group(&switch_device->kobj, &sm5714_muic_group);

	if (muic_data) {
		pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);

		cancel_delayed_work_sync(&muic_data->muic_debug_work);
		cancel_delayed_work_sync(&muic_data->muic_U200_work);
		disable_irq_wake(muic_data->i2c->irq);
		sm5714_muic_free_irqs(muic_data);
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
		vbus_notifier_unregister(&muic_data->vbus_nb);
#endif
		mutex_destroy(&muic_data->muic_mutex);
		mutex_destroy(&muic_data->afc_mutex);
		i2c_set_clientdata(muic_data->i2c, NULL);
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
		kfree(muic_data->man->muic_d);
#endif
		kfree(muic_data);
	}

	return 0;
}

static void sm5714_muic_shutdown(struct platform_device *pdev)
{
	struct sm5714_muic_data *muic_data = platform_get_drvdata(pdev);
	int ret;

	muic_data->shut_down = 1;

	cancel_delayed_work_sync(&muic_data->muic_debug_work);
	cancel_delayed_work_sync(&muic_data->muic_U200_work);

	pr_info("[%s:%s]\n", MUIC_DEV_NAME, __func__);
	if (!muic_data->i2c) {
		pr_err("[%s:%s] no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	pr_info("[%s:%s] open D+,D-,V_bus line\n", MUIC_DEV_NAME, __func__);
	ret = com_to_open(muic_data);
	if (ret < 0)
		pr_err("[%s:%s] fail to open mansw\n", MUIC_DEV_NAME, __func__);

	/* set auto sw mode before shutdown to make sure device goes into */
	/* LPM charging when TA or USB is connected during off state */
	pr_info("[%s:%s] muic auto detection enable\n",
			MUIC_DEV_NAME, __func__);

	ret = set_manual_sw(muic_data, MANSW_AUTOMATIC); /* auto:0  manual:1 */
	if (ret < 0) {
		pr_err("[%s:%s] fail to update reg\n", MUIC_DEV_NAME, __func__);
		return;
	}

#ifndef CONFIG_SEC_FACTORY
	/* If the device is in OTG state then BC 1.2 cannot work in BL state */
	sm5714_muic_set_bc12((void *)muic_data, 1);
#endif
}

#if defined CONFIG_PM
static int sm5714_muic_suspend(struct device *dev)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);

	muic_data->suspended = true;
	cancel_delayed_work_sync(&muic_data->muic_debug_work);

	return 0;
}

static int sm5714_muic_resume(struct device *dev)
{
	struct sm5714_muic_data *muic_data = dev_get_drvdata(dev);

	muic_data->suspended = false;

	if (muic_data->need_to_noti) {
		if (muic_data->attached_dev)
			muic_notifier_attach_attached_dev(
					muic_data->attached_dev);
		else
			muic_notifier_detach_attached_dev(
					muic_data->attached_dev);
		muic_data->need_to_noti = false;
	}
	schedule_delayed_work(&muic_data->muic_debug_work,
							msecs_to_jiffies(1500));

	return 0;
}
#else
#define sm5714_muic_suspend NULL
#define sm5714_muic_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(sm5714_muic_pm_ops, sm5714_muic_suspend,
			 sm5714_muic_resume);

static struct platform_driver sm5714_muic_driver = {
	.driver = {
		.name = "sm5714-muic",
		.owner	= THIS_MODULE,
		.of_match_table = sm5714_muic_match_table,
#ifdef CONFIG_PM
		.pm = &sm5714_muic_pm_ops,
#endif
	},
	.probe = sm5714_muic_probe,
	.remove = sm5714_muic_remove,
	.shutdown = sm5714_muic_shutdown,
};

static int __init sm5714_muic_init(void)
{
	return platform_driver_register(&sm5714_muic_driver);
}
late_initcall(sm5714_muic_init);

static void __exit sm5714_muic_exit(void)
{
	platform_driver_unregister(&sm5714_muic_driver);
}
module_exit(sm5714_muic_exit);

MODULE_DESCRIPTION("SM5714 USB Switch driver");
MODULE_LICENSE("GPL");
