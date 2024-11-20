/*
 * driver/muic/s2mf301.c - S2MF301 micro USB switch device driver
 *
 * Copyright (C) 2018 Samsung Electronics
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
#define pr_fmt(fmt)	"[MUIC] " fmt

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/muic/common/muic.h>
#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include <linux/mfd/slsi/s2mf301/s2mf301_log.h>
#include <linux/muic/slsi/s2mf301/s2mf301-muic.h>

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
#include <linux/muic/common/muic_sysfs.h>
#endif
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
#include <linux/muic/slsi/s2mf301/s2mf301-muic-hv.h>
#endif

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif

#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#include <linux/usb_notify.h>
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif
#if IS_ENABLED(CONFIG_MUIC_UART_SWITCH)
#include <mach/pinctrl-samsung.h>
#endif
#if IS_ENABLED(CONFIG_CP_UART_NOTI)
#include <soc/samsung/exynos-modem-ctrl.h>
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && !defined(CONFIG_BATTERY_GKI)
#include <linux/sec_batt.h>
#endif
#include <linux/usb/typec/common/pdic_param.h>

struct s2mf301_muic_data *static_data;
/* Prototypes of the Static symbols of s2mf301-muic */

int s2mf301_muic_bcd_rescan(struct s2mf301_muic_data *muic_data);
static int s2mf301_muic_detect_dev_bc1p2(struct s2mf301_muic_data *muic_data);
static void s2mf301_muic_handle_attached_dev(struct s2mf301_muic_data *muic_data);
void s2mf301_muic_get_detect_info(struct s2mf301_muic_data *muic_data);
static bool s2mf301_muic_is_opmode_typeC(struct s2mf301_muic_data *muic_data);
static void s2mf301_muic_set_water_state(struct s2mf301_muic_data *muic_data, bool en);
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static int s2mf301_muic_set_hiccup_mode(struct s2mf301_muic_data *muic_data, bool en);
static int s2mf301_muic_set_overheat_hiccup_mode(struct s2mf301_muic_data *muic_data, bool en);
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static int s2mf301_muic_rid_isr(void *_data);
#define PM_RID_OPS_FUNC(func) \
	((muic_data->pm_rid_ops.func) ?\
	(muic_data->pm_rid_ops.func(muic_data->pmeter)) :\
	(s2mf301_info("%s, %s in NULL\n", __func__, #func), -1))

#define PM_RID_OPS_FUNC_PARAM(func, param) \
	((muic_data->pm_rid_ops.func) ?\
	(muic_data->pm_rid_ops.func(muic_data->pmeter, param)) :\
	(s2mf301_info("%s, %s in NULL\n", __func__, #func), -1))

#define TOP_RID_OPS_FUNC(func) \
	((muic_data->top_rid_ops.func) ?\
	(muic_data->top_rid_ops.func(muic_data->top)) :\
	(s2mf301_info("%s, %s in NULL\n", __func__, #func), -1))

#define TOP_RID_OPS_FUNC_PARAM(func, param) \
	((muic_data->top_rid_ops.func) ?\
	(muic_data->top_rid_ops.func(muic_data->top, param)) :\
	(s2mf301_info("%s, %s in NULL\n", __func__, #func), -1))
#endif

#ifndef CONFIG_HV_MUIC_S2MF301_AFC
int muic_afc_set_voltage(int vol)
{
	/* To support sec_battery dependancy when AFC is disabled */
	return 0;
}
#endif

/*
 * Debuging functions
 */
static const char *dev_to_str(muic_attached_dev_t n)
{
	char *ret;

	switch (n) {
	ENUM_STR(ATTACHED_DEV_NONE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_USB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CDP_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_OTG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNDEFINED_CHARGING_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_DESKDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNKNOWN_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_DESKDOCK_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CARDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_ON_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_UART_ON_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_USB_OFF_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_JIG_USB_ON_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_SMARTDOCK_USB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AUDIODOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_MHL_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CHARGING_CABLE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_5V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_9V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_5V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_QC_CHARGER_9V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_HMT_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_VZW_ACC_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_USB_LANHUB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE2_CHG_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_MUIC_TA, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_ADAPTER_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TYPE3_CHARGER_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_NONE_TYPE3_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNSUPPORTED_ID_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_TIMEOUT_OPEN_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_WIRELESS_PAD_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_POWERPACK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNDEFINED_RANGE_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CHK_WATER_REQ, ret);
	ENUM_STR(ATTACHED_DEV_CHK_WATER_DRY_REQ, ret);
	ENUM_STR(ATTACHED_DEV_GAMEPAD_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_CHECK_OCP, ret);
	ENUM_STR(ATTACHED_DEV_RDU_TA_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_POGO_DOCK_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_POGO_DOCK_5V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_POGO_DOCK_9V_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_UNKNOWN_MUIC, ret);
	ENUM_STR(ATTACHED_DEV_NUM, ret);
	default:
		return "invalid";
	}
	return ret;
}

#if IS_ENABLED(CONFIG_MUIC_DEBUG)
#define MAX_LOG 25
#define READ 0
#define WRITE 1

static u8 s2mf301_log_cnt;
static u8 s2mf301_log[MAX_LOG][3];

static void s2mf301_reg_log(u8 reg, u8 value, u8 rw)
{
	s2mf301_log[s2mf301_log_cnt][0] = reg;
	s2mf301_log[s2mf301_log_cnt][1] = value;
	s2mf301_log[s2mf301_log_cnt][2] = rw;
	s2mf301_log_cnt++;
	if (s2mf301_log_cnt >= MAX_LOG)
		s2mf301_log_cnt = 0;
}

static void s2mf301_print_reg_log(void)
{
	int i;
	u8 reg, value, rw;
	char mesg[256] = "";

	for (i = 0; i < MAX_LOG; i++) {
		reg = s2mf301_log[s2mf301_log_cnt][0];
		value = s2mf301_log[s2mf301_log_cnt][1];
		rw = s2mf301_log[s2mf301_log_cnt][2];
		s2mf301_log_cnt++;

		if (s2mf301_log_cnt >= MAX_LOG)
			s2mf301_log_cnt = 0;
		sprintf(mesg + strlen(mesg), "%x(%x)%x ", reg, value, rw);
	}
	s2mf301_info("%s %s\n", __func__, mesg);
}

void s2mf301_read_reg_dump(struct s2mf301_muic_data *muic, char *mesg)
{
	u8 val;

	mutex_lock(&muic->muic_mutex);
	s2mf301_read_reg(muic->i2c, S2MF301_REG_MUIC_INT, &val);
	sprintf(mesg + strlen(mesg), "INT:%x ", val);
	s2mf301_read_reg(muic->i2c, S2MF301_REG_MUIC_INT_MASK, &val);
	sprintf(mesg + strlen(mesg), "IM:%x ", val);
	s2mf301_read_reg(muic->i2c, S2MF301_REG_MUIC_CTRL1, &val);
	sprintf(mesg + strlen(mesg), "CTRL1:%x ", val);
	s2mf301_read_reg(muic->i2c, S2MF301_REG_MANUAL_SW_CTRL, &val);
	sprintf(mesg + strlen(mesg), "SW_CTRL:%x ", val);
	s2mf301_read_reg(muic->i2c, S2MF301_REG_DEVICE_TYP1, &val);
	sprintf(mesg + strlen(mesg), "DT1:%x ", val);
	s2mf301_read_reg(muic->i2c, S2MF301_REG_DEVICE_TYP2, &val);
	sprintf(mesg + strlen(mesg), "DT2:%x ", val);
	mutex_unlock(&muic->muic_mutex);
}

void s2mf301_print_reg_dump(struct s2mf301_muic_data *muic_data)
{
	char mesg[256] = "";
	s2mf301_read_reg_dump(muic_data, mesg);
	s2mf301_info("%s %s\n", __func__, mesg);
}

void s2mf301_read_reg_dump_otp(struct s2mf301_muic_data *muic_data)
{
	int i;
	u8 val;
	s2mf301_info("%s=======================START\n", __func__);
	for (i = 0x6D; i <= 0xD9; i++) {
		s2mf301_read_reg(muic_data->i2c, i, &val);
		s2mf301_info("0x%X : 0x%X\n", i, val);
	}
	s2mf301_info("%s=======================END\n", __func__);
}
#endif

/*
 * Unit Functions
 */
int s2mf301_i2c_read_byte(struct i2c_client *client, u8 command)
{
	int ret = 0;
	int retry = 0;
	u8 data = 0;

	ret = s2mf301_read_reg(client, command, &data);
	while (ret < 0) {
		s2mf301_info("failed to read reg(0x%x) retry(%d)\n", command, retry);
		if (retry > 10) {
			s2mf301_err("%s  retry failed!!\n", __func__);
			break;
		}
		msleep(100);
		ret = s2mf301_read_reg(client, command, &data);
		retry++;
	}

#if IS_ENABLED(DEBUG_MUIC)
	s2mf301_reg_log(command, ret, retry << 1 | READ);
#endif
	return data;
}
EXPORT_SYMBOL(s2mf301_i2c_read_byte);

int s2mf301_i2c_write_byte(struct i2c_client *client, u8 command, u8 value)
{
	int ret_r = 0;
	int ret_w = 0;
	int retry = 0;
	u8 written = 0;

	ret_w = s2mf301_write_reg(client, command, value);
	while (ret_w < 0) {
		s2mf301_info("failed to write reg(0x%x) retry(%d)\n", command, retry);
		ret_r = s2mf301_read_reg(client, command, &written);
		if (ret_r < 0)
			s2mf301_err("%s reg(0x%x)\n", __func__, command);
		msleep(100);
		ret_w = s2mf301_write_reg(client, command, value);
		retry++;
	}
#if IS_ENABLED(DEBUG_MUIC)
	s2mf301_reg_log(command, value, retry << 1 | WRITE);
#endif
	return ret_w;
}

static int _s2mf301_i2c_guaranteed_wbyte(struct i2c_client *client,
					u8 command, u8 value)
{
	int ret;
	int retry = 0;
	int written;

	ret = s2mf301_i2c_write_byte(client, command, value);
	written = s2mf301_i2c_read_byte(client, command);
	while (written != value) {
		s2mf301_info("reg(0x%x): written(0x%x) != value(0x%x)\n",
			command, written, value);
		if (retry > 10) {
			s2mf301_err("%s  retry failed!!\n", __func__);
			break;
		}
		msleep(100);
		retry++;
		ret = s2mf301_i2c_write_byte(client, command, value);
		written = s2mf301_i2c_read_byte(client, command);
	}
	return ret;
}

static int _s2mf301_i2c_update_bit(struct i2c_client *i2c,
			u8 reg, u8 mask, u8 shift, u8 value)
{
	int ret;
	u8 reg_val = 0;

	reg_val = s2mf301_i2c_read_byte(i2c, reg);
	reg_val &= ~mask;
	reg_val |= value << shift;
	ret = s2mf301_i2c_write_byte(i2c, reg, reg_val);
	s2mf301_info("%s reg(0x%x) value(0x%x)\n", __func__, reg, reg_val);
	if (ret < 0)
		s2mf301_err("%s  Reg = 0x%X, mask = 0x%X, val = 0x%X write err : %d\n",
				__func__, reg, mask, value, ret);

	return ret;
}

#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
static void _s2mf301_muic_set_chg_det(struct s2mf301_muic_data *muic_data,
		bool enable)
{
	struct muic_share_data *sdata = muic_data->sdata;
	struct i2c_client *i2c = muic_data->i2c;
	u8 r_val = 0, w_val = 0;

	if (sdata->is_pdic_probe == false) {
		s2mf301_err("%s pdic driver is not probed yet\n", __func__);
		return;
	}

	r_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MUIC_ETC);
	if (enable)
		w_val = r_val & ~(S2MF301_MUIC_MUIC_ETC_REG_CHG_DET_OFF_MASK);
	else
		w_val = r_val | S2MF301_MUIC_MUIC_ETC_REG_CHG_DET_OFF_MASK;

	if (w_val != r_val) {
		s2mf301_info("%s en(%d)\n", __func__, enable);
		s2mf301_i2c_write_byte(i2c, S2MF301_REG_MUIC_ETC, w_val);
	}
}
#endif

static void s2mf301_muic_vbus_onoff(struct s2mf301_muic_data *muic_data)
{
	union power_supply_propval val;

	if (!muic_data->psy_pd)
		muic_data->psy_pd = power_supply_get_by_name("usbpd-manager");

	if (!muic_data->psy_pd) {
		s2mf301_info("%s, Fail to get psy_pd, after 1sec\n", __func__);
		schedule_delayed_work(&muic_data->pcp_clk_work, msecs_to_jiffies(1000));
		return;
	}

	val.intval = 0;

	if (muic_data->psy_pd)
		muic_data->psy_pd->desc->set_property(muic_data->psy_pd,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_VCHGIN, &val);

	return;
}

static void s2mf301_muic_pcp_clk_work(struct work_struct *work)
{
	struct s2mf301_muic_data *muic_data =
		container_of(work, struct s2mf301_muic_data, pcp_clk_work.work);
	union power_supply_propval val;

	s2mf301_info("%s, PCP_CLK for FS USB is delayed, mode(%d)\n", __func__, muic_data->pcp_clk_delayed);

	if (!muic_data->psy_pd)
		muic_data->psy_pd = power_supply_get_by_name("usbpd-manager");

	if (!muic_data->psy_pd) {
		s2mf301_info("%s, Fail to get psy_pd, after 1sec\n", __func__);
		schedule_delayed_work(&muic_data->pcp_clk_work, msecs_to_jiffies(1000));
		return;
	}

	val.intval = muic_data->pcp_clk_delayed;

	if (muic_data->psy_pd)
		muic_data->psy_pd->desc->set_property(muic_data->psy_pd,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_PCP_CLK, &val);
}

static void s2mf301_muic_ldo_sel(struct s2mf301_muic_data *muic_data, enum s2mf301_muic_enable mode)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val;
	union power_supply_propval val;

	if (mode > S2MF301_ENABLE)
		return;

	s2mf301_info("%s, %s mode\n", __func__,
		(mode == S2MF301_ENABLE ? "USB FS" : "Default"));

	if (!muic_data->psy_pd)
		muic_data->psy_pd = power_supply_get_by_name("usbpd-manager");

	if (!muic_data->psy_pd) {
		s2mf301_info("%s, Fail to get psy_pd, after 1sec\n", __func__);
		muic_data->pcp_clk_delayed = mode;
		s2mf301_info("%s, mode(%d), pdp_clk_delayed(%d)\n", __func__, mode, muic_data->pcp_clk_delayed);
		schedule_delayed_work(&muic_data->pcp_clk_work, msecs_to_jiffies(1000));
	} else {
		if (mode)
			val.intval = 1;
		else
			val.intval = 0;

		if (muic_data->psy_pd)
			muic_data->psy_pd->desc->set_property(muic_data->psy_pd,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_PCP_CLK, &val);
	}

	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_OVPSW_LDO);
	switch (mode) {
	case S2MF301_ENABLE:
		reg_val &= ~S2MF301_MUIC_OVPSW_LDO_OVPSW_LDO_SEL_MASK;
		reg_val |= 0x1F;
		break;
	case S2MF301_DISABLE:
		reg_val &= ~S2MF301_MUIC_OVPSW_LDO_OVPSW_LDO_SEL_MASK;
		reg_val |= 0x19;
		break;
	}
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_OVPSW_LDO, reg_val);
	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_OVPSW_LDO);
	s2mf301_info("%s, 0x2E(0x%02x)", __func__, reg_val);

	return;
}

static int _s2mf301_muic_sel_path(struct s2mf301_muic_data *muic_data,
	t_path_data path_data)
{
	int ret = 0;
	u8 reg_val1, reg_val2;

	reg_val1 = s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_MANUAL_SW_CTRL);
	reg_val2 = reg_val1 & ~S2MF301_MANUAL_SW_CTRL_SWITCH_MASK;

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	if (muic_data->is_hiccup_mode)
		reg_val2 |= S2MF301_MANUAL_SW_CTRL_HICCUP;
	else {
#endif
		switch (path_data) {
		case S2MF301_PATH_USB:
			reg_val2 |= S2MF301_MANUAL_SW_CTRL_USB;
			break;
		case S2MF301_PATH_UART_AP:
			reg_val2 |= S2MF301_MANUAL_SW_CTRL_UART1;
			break;
		case S2MF301_PATH_UART_CP:
#if IS_ENABLED(CONFIG_PMU_UART_SWITCH)
			reg_val2 |= S2MF301_MANUAL_SW_CTRL_UART1;
#else
			reg_val2 |= S2MF301_MANUAL_SW_CTRL_UART2;
#endif
			break;
		case S2MF301_PATH_OPEN:
		default:
			reg_val2 &= ~S2MF301_MANUAL_SW_CTRL_SWITCH_MASK;
			break;
		}
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	}
#endif
	if (reg_val1 != reg_val2) {
		ret = s2mf301_i2c_write_byte(muic_data->i2c, S2MF301_REG_MANUAL_SW_CTRL, reg_val2);
		reg_val2 = s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_MANUAL_SW_CTRL);
		s2mf301_info("%s manual_sw_ctrl(%#x->%#x)\n", __func__, reg_val1, reg_val2);
	} else
		s2mf301_info("%s Skip to set same path val(%#x)\n", __func__, reg_val1);

	return ret;
}

static int _s2mf301_muic_set_path_mode(struct s2mf301_muic_data *muic_data,
	t_mode_data mode_data)
{
	int ret = 0;
	u8 reg_val;

	s2mf301_info("%s new mode(%d)\n", __func__, mode_data);
	reg_val = s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_MUIC_CTRL1);

	switch (mode_data) {
	case S2MF301_MODE_MANUAL:
			reg_val &= ~S2MF301_MUIC_MUIC_CTRL1_MANUAL_SW_MASK;
		break;
	case S2MF301_MODE_AUTO:
	default:
			reg_val |= S2MF301_MUIC_MUIC_CTRL1_MANUAL_SW_MASK;
		break;
	}

	ret = s2mf301_i2c_write_byte(muic_data->i2c, S2MF301_REG_MUIC_CTRL1, reg_val);

	return ret;
}

static int _s2mf301_muic_control_rid_adc(struct s2mf301_muic_data *muic_data, bool enable)
{
	s2mf301_info("%s (%s)\n", __func__, enable ? "Enable" : "Disable");

	PM_RID_OPS_FUNC_PARAM(control_rid_adc, enable);

	return 0;
}

static int _s2mf301_muic_set_bcd_rescan_reg(struct s2mf301_muic_data *muic_data)
{
	u8 reg_val = 0;
	reg_val = s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_RESET_BCD_RESCAN);
	reg_val |= S2MF301_MUIC_RESET_N_BCD_RESCAN_BCD_RESCAN_MASK;
	s2mf301_i2c_write_byte(muic_data->i2c, S2MF301_REG_RESET_BCD_RESCAN, reg_val);
	return 0;
}

static inline int _s2mf301_muic_get_rid_adc(struct s2mf301_muic_data *muic_data)
{
	return PM_RID_OPS_FUNC(get_rid_adc);
}

static int _s2mf301_muic_com_to_uart(struct s2mf301_muic_data *muic_data, int path)
{
	int ret = 0;
	struct muic_share_data *sdata = muic_data->sdata;

	if (sdata->is_rustproof) {
		s2mf301_info("%s rustproof mode\n", __func__);
		return ret;
	}

	if (path == MUIC_PATH_UART_AP) {
		ret = _s2mf301_muic_sel_path(muic_data, S2MF301_PATH_UART_AP);
#if IS_ENABLED(CONFIG_CP_UART_NOTI)
		send_uart_noti_to_modem(MODEM_CTRL_UART_AP);
#endif
	} else {
		ret = _s2mf301_muic_sel_path(muic_data, S2MF301_PATH_UART_CP);
#if IS_ENABLED(CONFIG_CP_UART_NOTI)
		send_uart_noti_to_modem(MODEM_CTRL_UART_CP);
#endif
	}

	return ret;
}

int _s2mf301_muic_set_jig_on(struct s2mf301_muic_data *muic_data)
{
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	struct muic_share_data *sdata = muic_data->sdata;
	bool en = sdata->is_jig_on;
	int ret = 0;

	TOP_RID_OPS_FUNC_PARAM(set_jig_on, en);

	return ret;
#else
	s2mf301_err("%s: Skip the jig control, Not Factory Mode.\n", __func__);
	return 0;
#endif
}

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
int _s2mf301_muic_recheck_adc(struct s2mf301_muic_data *muic_data)
{
	_s2mf301_muic_control_rid_adc(muic_data, MUIC_DISABLE);
	usleep_range(10000, 12000);
	_s2mf301_muic_control_rid_adc(muic_data, MUIC_ENABLE);
	msleep(200);
	return _s2mf301_muic_get_rid_adc(muic_data);
}

int s2mf301_muic_refresh_adc(struct s2mf301_muic_data *muic_data)
{
	int adc = 0;
	u8 rid_is_enable = 0;

	rid_is_enable = PM_RID_OPS_FUNC(rid_is_enable);

	if (!rid_is_enable) {
		s2mf301_info("%s, enable the RID\n", __func__);
		_s2mf301_muic_control_rid_adc(muic_data, MUIC_ENABLE);
		msleep(35);
	}

	adc = _s2mf301_muic_get_rid_adc(muic_data);
	s2mf301_info("%s, adc : 0x%X\n", __func__, adc);

	if (!rid_is_enable)
		_s2mf301_muic_control_rid_adc(muic_data, MUIC_DISABLE);

	return adc;
}
#endif

static inline int _s2mf301_muic_get_vbus_state(struct s2mf301_muic_data *muic_data)
{
	s2mf301_info("%s+\n", __func__);
	return (s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_DEVICE_TYP2)
		& S2MF301_MUIC_DEVICE_TYPE2_VBUS_WAKEUP_MASK) >> S2MF301_MUIC_DEVICE_TYPE2_VBUS_WAKEUP_SHIFT;
}

void s2mf301_muic_get_detect_info(struct s2mf301_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	struct muic_share_data *sdata = muic_data->sdata;

	muic_data->reg[ADC] = _s2mf301_muic_get_rid_adc(muic_data);
	muic_data->reg[DEVICE_TYPE1] = s2mf301_i2c_read_byte(i2c, S2MF301_REG_DEVICE_TYP1);
	s2mf301_info("%s: DEVICE TYPE 1\n", __func__);
	muic_data->reg[DEVICE_TYPE2] = s2mf301_i2c_read_byte(i2c, S2MF301_REG_DEVICE_TYP2);
	s2mf301_info("%s: DEVICE TYPE 2\n", __func__);
	muic_data->vbvolt = sdata->vbus_state = _s2mf301_muic_get_vbus_state(muic_data);
	muic_data->adc = muic_data->reg[ADC];

	s2mf301_info("dev[1:0x%02x, 2:0x%02x]\n", muic_data->reg[DEVICE_TYPE1],
		muic_data->reg[DEVICE_TYPE2]);
	s2mf301_info("adc:0x%02x, vbvolt:0x%02x\n",
		muic_data->adc, muic_data->vbvolt);
}

int s2mf301_muic_bcd_rescan(struct s2mf301_muic_data *muic_data)
{
	int ret = 0;

	s2mf301_info("%s call\n", __func__);
	muic_data->rescan_cnt++;
	/* muic mux switch open */
	ret = _s2mf301_muic_sel_path(muic_data, S2MF301_PATH_OPEN);
	if (ret < 0)
		s2mf301_err("%s, fail to open mansw\n", __func__);

	_s2mf301_muic_set_bcd_rescan_reg(muic_data);

	return 0;
}

static void s2mf301_muic_dcd_recheck(struct work_struct *work)
{
	struct s2mf301_muic_data *muic_data =
	    container_of(work, struct s2mf301_muic_data, dcd_recheck.work);
	struct muic_share_data *sdata = muic_data->sdata;

	/* Driver re-detects the rescan type. */
	int det_ret = S2MF301_DETECT_NONE;

	if (muic_platform_get_pdic_cable_state(sdata)) {
		s2mf301_info("%s Tried to rescan, but pd type detected.\n", __func__);
		return;
	}

	mutex_lock(&muic_data->bcd_rescan_mutex);
	if (!muic_data->vbvolt)
		goto skip_dcd_recheck;

	s2mf301_muic_bcd_rescan(muic_data);
	msleep(650);
	s2mf301_muic_get_detect_info(muic_data);

	if (!muic_data->vbvolt || muic_platform_get_pdic_cable_state(sdata))
		goto skip_dcd_recheck;

#if IS_ENABLED(CONFIG_S2MF301_IFCONN_HOUSE_NOT_GND)
	_s2mf301_muic_set_rescan_status(muic_data, false);
#endif
	/* Detect Type & Handle the result */
	det_ret = s2mf301_muic_detect_dev_bc1p2(muic_data);
	if (det_ret == S2MF301_DETECT_NONE) {
		s2mf301_err("%s abnormal detection : set general TA\n", __func__);
		muic_data->new_dev = ATTACHED_DEV_TA_MUIC;
	}
	s2mf301_muic_handle_attached_dev(muic_data);

skip_dcd_recheck:
	mutex_unlock(&muic_data->bcd_rescan_mutex);
}

static void s2mf301_muic_rescan_validity_checker(struct work_struct *work)
{
	struct s2mf301_muic_data *muic_data =
		container_of(work, struct s2mf301_muic_data, rescan_validity_checker.work);
	struct muic_share_data *sdata = muic_data->sdata;

	s2mf301_info("%s entered\n", __func__);

	if (!_s2mf301_muic_get_vbus_state(muic_data)) {
		return;
	} else if (sdata->is_bypass) {
		s2mf301_info("%s is_bypass(%d)\n", __func__, sdata->is_bypass);
		return;
	}

	if (!MUIC_IS_ATTACHED(sdata->attached_dev))
		s2mf301_info("%s detected dev(%s)\n", __func__, dev_to_str(sdata->attached_dev));
}

/*
 * Interface Functions
 */
#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static int s2mf301_ops_get_adc(void *mdata)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	int ret = 0;

	mutex_lock(&muic_data->switch_mutex);

	ret = _s2mf301_muic_get_rid_adc(muic_data);

	mutex_unlock(&muic_data->switch_mutex);

	return ret;
}
#endif

#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
static void s2mf301_ops_prswap_work(void *mdata, int mode)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	struct muic_share_data *sdata = muic_data->sdata;

	s2mf301_info("%s+ dev(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	if (sdata->attached_dev == ATTACHED_DEV_USB_MUIC
			|| sdata->attached_dev == ATTACHED_DEV_OTG_MUIC
			|| sdata->attached_dev == ATTACHED_DEV_TIMEOUT_OPEN_MUIC) {
		s2mf301_err("%s(%d) invalid status\n", __func__, __LINE__);
		goto work_done;
	}

	mutex_lock(&muic_data->muic_mutex);
	_s2mf301_muic_set_chg_det(muic_data, MUIC_DISABLE);
	_s2mf301_muic_sel_path(muic_data, S2MF301_PATH_USB);

	switch (mode) {
	case MUIC_PRSWAP_TO_SINK:
		muic_platform_handle_attach(sdata, ATTACHED_DEV_USB_MUIC);
		break;
	case MUIC_PRSWAP_TO_SRC:
		sdata->attached_dev = ATTACHED_DEV_OTG_MUIC;
		break;
	default:
		s2mf301_err("%s(%d) invalid value\n", __func__, __LINE__);
		break;
	}
	mutex_unlock(&muic_data->muic_mutex);
work_done:
	s2mf301_info("%s- dev(%s)\n", __func__, dev_to_str(sdata->attached_dev));
}
#endif

static void s2mf301_ops_set_bypass(void *mdata)
{
	struct s2mf301_muic_data *muic_data = mdata;
	struct muic_share_data *sdata;

	if (muic_data == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return;
	}

	sdata = muic_data->sdata;

	if (sdata == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return;
	}

	s2mf301_info("%s, call detach\n", __func__);
	muic_platform_handle_detach(sdata);
}

static int s2mf301_ops_com_to_open(void *mdata)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	int ret = 0;

	mutex_lock(&muic_data->switch_mutex);

	s2mf301_muic_ldo_sel(muic_data, S2MF301_DISABLE);

	ret = _s2mf301_muic_sel_path(muic_data, S2MF301_PATH_OPEN);
	if (ret)
		s2mf301_err("%s set_com_open err\n", __func__);

	mutex_unlock(&muic_data->switch_mutex);

	return ret;
}

static int s2mf301_ops_com_to_usb(void *mdata, int path)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	int ret = 0;

	mutex_lock(&muic_data->switch_mutex);

	s2mf301_muic_ldo_sel(muic_data, S2MF301_ENABLE);

	ret = _s2mf301_muic_sel_path(muic_data, S2MF301_PATH_USB);
	if (ret)
		s2mf301_err("%s set_com_usb err\n", __func__);

	mutex_unlock(&muic_data->switch_mutex);

	return ret;
}

static int s2mf301_ops_com_to_uart(void *mdata, int path)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	int ret = 0;

	mutex_lock(&muic_data->switch_mutex);

	s2mf301_muic_ldo_sel(muic_data, S2MF301_DISABLE);

	ret = _s2mf301_muic_com_to_uart(muic_data, path);
	if (ret)
		s2mf301_err("%s set_com_uart err\n", __func__);

	mutex_unlock(&muic_data->switch_mutex);

	return ret;
}

static int s2mf301_ops_get_sbu_ovp_state(void *mdata)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	u8 reg = s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_OVP_STATUS);

	pr_info("%s, OVP Status [0x%x]\n", __func__, reg & 0xc0);

	return (reg & 0xc0) ? true : false;
}

static int s2mf301_ops_get_vbus_state(void *mdata)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;

	return _s2mf301_muic_get_vbus_state(muic_data);
}

#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
static void s2mf301_ops_set_chg_det(void *mdata, bool enable)

{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;

	_s2mf301_muic_set_chg_det(muic_data, enable);
}
#endif

static void s2mf301_ops_set_water_state(void *mdata, bool en)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->muic_ws);

	s2mf301_muic_set_water_state(muic_data, en);

	__pm_relax(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);
}

#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
static int s2mf301_muic_get_vdnmon(struct s2mf301_muic_data *muic_data)
{
	u8 vdnmon = s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_AFC_STATUS);

	s2mf301_info("%s(%d) vdnmon(%#x)\n", __func__, __LINE__, vdnmon);
	return (vdnmon >> S2MF301_MUIC_AFC_STATUS_VDNMon_SHIFT) & 0x1;
}

static void s2mf301_muic_set_dn_ready_for_killer(struct s2mf301_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int i;
	u8 vdnmon, reg_val;

	vdnmon = s2mf301_muic_get_vdnmon(muic_data);
	if (!vdnmon)
		return;

	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_AFC_CTRL1);
	for (i = 0; i < 10; i++) {
		reg_val = (S2MF301_MUIC_AFC_CTRL1_AFC_EN_MASK | S2MF301_MUIC_AFC_CTRL1_DPDNVd_EN_MASK |
				S2MF301_MUIC_AFC_DPVd_GND | S2MF301_MUIC_AFC_DNVd_GND);
		s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL1, reg_val);

		usleep_range(20000, 21000);

		reg_val = (S2MF301_MUIC_AFC_CTRL1_AFC_EN_MASK | S2MF301_MUIC_AFC_CTRL1_DPDNVd_EN_MASK |
				S2MF301_MUIC_AFC_DPVd_HIZ | S2MF301_MUIC_AFC_DNVd_HIZ);
		s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL1, reg_val);

		usleep_range(10000, 11000);

		vdnmon = s2mf301_muic_get_vdnmon(muic_data);
		if (!vdnmon)
			break;
	}
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL1,
			(S2MF301_MUIC_AFC_CTRL1_AFC_EN_MASK | S2MF301_MUIC_AFC_CTRL1_DPDNVd_EN_MASK));
}

static int s2mf301_muic_detect_usb_killer(struct s2mf301_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val = 0;
	u8 afc_otp6, vdnmon, afc_otp3, muic_ctrl1;
	int ret = MUIC_NORMAL_OTG;

	s2mf301_info("%s entered\n", __func__);

	/* Set Data Path to Open. */
	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MANUAL_SW_CTRL);
	if (reg_val & S2MF301_MANUAL_SW_CTRL_SWITCH_MASK) {
		ret = _s2mf301_muic_sel_path(muic_data, S2MF301_PATH_OPEN);
		msleep(50);
	}

	/* AFC Block Enable & INT Masking */
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_INT_MASK, 0xff);

	reg_val = (S2MF301_MUIC_AFC_CTRL1_AFC_EN_MASK | S2MF301_MUIC_AFC_CTRL1_DPDNVd_EN_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL1, reg_val);

	s2mf301_muic_set_dn_ready_for_killer(muic_data);

	usleep_range(10000, 11000);

	muic_ctrl1 = reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MUIC_CTRL1);
	reg_val |= (S2MF301_MUIC_MUIC_CTRL1_TX_DP_RDN_MASK | S2MF301_MUIC_MUIC_CTRL1_TX_DM_RDN_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_MUIC_CTRL1, reg_val);

	/* 1st check */
	reg_val = (S2MF301_MUIC_AFC_CTRL2_DP06EN_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL2, reg_val);

	msleep(80);

	vdnmon = s2mf301_muic_get_vdnmon(muic_data);
	if (!vdnmon) {
		s2mf301_info("%s, 1st chk: Normal OTG.", __func__);
		ret = MUIC_NORMAL_OTG;
		goto exit_chk;
	}
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_MUIC_CTRL1, muic_ctrl1);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL2, 0x00);

	/* 2nd check */
	afc_otp6 = s2mf301_i2c_read_byte(i2c, S2MF301_REG_AFC_OTP6);
	reg_val = (afc_otp6 | S2MF301_MUIC_AFC_OTP6_CTRL_IDM_ON_REG_SEL_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_OTP6, reg_val);

	reg_val = (S2MF301_MUIC_AFC_CTRL2_DNRESEN_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL2, reg_val);

	reg_val = (S2MF301_MUIC_AFC_CTRL1_CTRL_IDM_ON_MASK | S2MF301_MUIC_AFC_CTRL1_AFC_EN_MASK |
			S2MF301_MUIC_AFC_CTRL1_DPDNVd_EN_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL1, reg_val);

	afc_otp3 = s2mf301_i2c_read_byte(i2c, S2MF301_REG_AFC_OTP3);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_OTP3,
			S2MF301_MUIC_AFC_OTP3_COMP_REF_SEL_0p3V | S2MF301_MUIC_AFC_OTP3_HCOMP_REF_SEL_0p5V);

	muic_ctrl1 = reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MUIC_CTRL1);
	reg_val &= ~(S2MF301_MUIC_MUIC_CTRL1_TX_DP_RDN_MASK | S2MF301_MUIC_MUIC_CTRL1_TX_DM_RDN_MASK);
	reg_val |= (S2MF301_MUIC_MUIC_CTRL1_TX_DP_RDN_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_MUIC_CTRL1, reg_val);
	usleep_range(20000, 21000);

	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_AFC_STATUS);
	s2mf301_info("%s 2nd chk: AFC_STATUS(%#x)\n", __func__, reg_val);
	reg_val = (reg_val & (S2MF301_MUIC_AFC_STATUS_KILLER_CHECK_MASK)) >> S2MF301_MUIC_AFC_STATUS_KILLER_CHECK_SHIFT;
	if (reg_val == 0x02) {
		s2mf301_info("%s, USB Killer Condition.", __func__);
		ret = MUIC_ABNORMAL_OTG;
	}

	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_OTP6, afc_otp6);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_OTP3, afc_otp3);
exit_chk:
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_MUIC_CTRL1, muic_ctrl1);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_INT_MASK, 0x00);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL1, 0x0);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_CTRL2, 0x0);

	return ret;
}

static int s2mf301_ops_check_usb_killer(void *mdata)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	int ret = MUIC_NORMAL_OTG, i;

	if (muic_data->is_water_detected) {
		s2mf301_err("%s water is detected\n", __func__);
		return MUIC_ABNORMAL_OTG;
	}

	for (i = 0; i < 3; i++) {
		ret = s2mf301_muic_detect_usb_killer(muic_data);
		if (ret == MUIC_NORMAL_OTG)
			return ret;
		msleep(150);
	}

	s2mf301_info("%s, USB Killer is detected.", __func__);
	return ret;
}
#endif

static int s2mf301_ops_set_gpio_uart_sel(void *mdata, int uart_path)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;

	return s2mf301_set_gpio_uart_sel(muic_data, uart_path);
}

static int s2mf301_ops_set_jig_ctrl_on(void *mdata)
{
	struct s2mf301_muic_data *muic_data =
		(struct s2mf301_muic_data *)mdata;

	return _s2mf301_muic_set_jig_on(muic_data);
}

#if IS_ENABLED(CONFIG_MUIC_COMMON_SYSFS)
static int s2mf301_ops_show_register(void *mdata, char *mesg)
{
#if IS_ENABLED(CONFIG_MUIC_DEBUG)
	struct s2mf301_muic_data *muic_data =
		(struct s2mf301_muic_data *)mdata;

	if (mesg != NULL)
		s2mf301_read_reg_dump(muic_data, mesg);
	s2mf301_info("%s %s\n", __func__, mesg);
#endif
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static int s2mf301_ops_set_hiccup_mode(void *mdata, bool val)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;

	s2mf301_muic_set_hiccup_mode(muic_data, val);
	return 0;
}

static int s2mf301_ops_set_overheat_hiccup_mode(void *mdata, bool val)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;

	return s2mf301_muic_set_overheat_hiccup_mode(muic_data, val);
}

static int s2mf301_ops_get_hiccup_mode(void *mdata)
{
	struct s2mf301_muic_data *muic_data = (struct s2mf301_muic_data *)mdata;
	return muic_data->is_hiccup_mode;
}
#endif

int s2mf301_set_gpio_uart_sel(struct s2mf301_muic_data *muic_data, int uart_sel)
{
	const char *mode;
	struct muic_share_data *sdata = muic_data->sdata;
#if !IS_ENABLED(CONFIG_MUIC_UART_SWITCH)
	int uart_sel_gpio = sdata->gpio_uart_sel;
	int uart_sel_val;
	int ret;

	if (!gpio_is_valid(uart_sel_gpio)) {
		s2mf301_info("%s. skip. GPIO_UART_SEL(%d)\n", __func__, uart_sel_gpio);
		goto skip;
	}

	ret = gpio_request(uart_sel_gpio, "GPIO_UART_SEL");
	if (ret) {
		s2mf301_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, 1);
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, 0);
		break;
	default:
		mode = "Error";
		break;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	gpio_free(uart_sel_gpio);

	s2mf301_info("%s, GPIO_UART_SEL(%d)=%c\n",
		mode, uart_sel_gpio, (uart_sel_val == 0 ? 'L' : 'H'));
skip:
#else
	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_rxd,
			       PINCFG_PACK(PINCFG_TYPE_FUNC, 0x2));
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_txd,
			       PINCFG_PACK(PINCFG_TYPE_FUNC, 0x2));
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_rxd,
			       PINCFG_PACK(PINCFG_TYPE_FUNC, 0x3));
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_txd,
			       PINCFG_PACK(PINCFG_TYPE_FUNC, 0x3));
		break;
	default:
		mode = "Error";
		break;
	}

	s2mf301_info("%s %s\n", __func__, mode);
#endif /* CONFIG_MUIC_UART_SWITCH */
	return 0;
}

static int s2mf301_muic_reg_init(struct s2mf301_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0, data = 0;
	u8 reg_val = 0;
	u8 r_val = 0, w_val = 0;

	s2mf301_info("%s\n", __func__);

	s2mf301_muic_get_detect_info(muic_data);

	s2mf301_i2c_write_byte(i2c, S2MF301_REG_MUIC_INT_MASK, INT_MUIC_MASK);
	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MUIC_INT_MASK);
	s2mf301_info("%s, MUIC_INT_MASK(0x%2x)\n", __func__, reg_val);

	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_TIMER_SET1);
	reg_val &= ~S2MF301_MUIC_TIMER_SET1_DCDTMRSET_MASK;
	reg_val |= S2MF301_MUIC_TIMER_SET1_DCDTMRSET_600MS;
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_TIMER_SET1, reg_val);

	/* Set VDAT_REF 0.3V */
	r_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_AFC_OTP3);
	w_val = r_val & ~S2MF301_MUIC_AFC_OTP3_COMP_REF_SEL_MASK;
	w_val |= S2MF301_MUIC_AFC_OTP3_COMP_REF_SEL_0p3V;
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_AFC_OTP3, w_val);
	s2mf301_info("%s AFC_OTP3 %#x->%#x\n", __func__, r_val, w_val);

	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MUIC_CTRL2);
	reg_val &= ~(0x1);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_MUIC_CTRL2, reg_val);
	reg_val = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MUIC_CTRL2);
	s2mf301_info("%s, MUIC_CTRL2 : 0x%2x, enable interrupt pin\n", __func__, reg_val);

	/* for usb killer */
	_s2mf301_i2c_update_bit(muic_data->i2c, S2MF301_REG_MUIC_CTRL2,
			S2MF301_MUIC_MUIC_CTRL2_USBKILL_OTG_EN_MASK,
			S2MF301_MUIC_MUIC_CTRL2_USBKILL_OTG_EN_SHIFT,
			0x1);

#if IS_ENABLED(CONFIG_MUIC_S2MF301_ENABLE_AUTOSW)
	reg_val = S2MF301_MUIC_MUIC_CTRL1_SWITCH_OPENB_MASK | S2MF301_MUIC_MUIC_CTRL1_FAC_WAIT_MASK
		| S2MF301_MUIC_MUIC_CTRL1_MANUAL_SW_MASK;
#else
	reg_val = S2MF301_MUIC_MUIC_CTRL1_SWITCH_OPENB_MASK | S2MF301_MUIC_MUIC_CTRL1_FAC_WAIT_MASK;
#endif
	ret = _s2mf301_i2c_guaranteed_wbyte(i2c, S2MF301_REG_MUIC_CTRL1, reg_val);
	if (ret < 0)
		s2mf301_err("failed to write ctrl(%d)\n", ret);

	data = s2mf301_i2c_read_byte(i2c, S2MF301_REG_MANUAL_SW_CTRL);
	data |= (S2MF301_MUIC_MUIC_SW_CTRL_GATESW_ON_MASK |
			S2MF301_MUIC_MUIC_SW_CTRL_REVD_MASK |
			S2MF301_MUIC_MUIC_SW_CTRL_USB_OVP_EN_MASK);
	s2mf301_i2c_write_byte(i2c, S2MF301_REG_MANUAL_SW_CTRL, data);
	s2mf301_info("%s init path(0x%x)\n", __func__, data);
	if (data & S2MF301_MANUAL_SW_CTRL_SWITCH_MASK)
		_s2mf301_muic_sel_path(muic_data, S2MF301_PATH_OPEN);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_handle((!!muic_data->vbvolt) ? STATUS_VBUS_HIGH : STATUS_VBUS_LOW);
#endif /* CONFIG_VBUS_NOTIFIER */

	return ret;
}

static int s2mf301_muic_detect_dev_bc1p2(struct s2mf301_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	struct muic_ic_data *ic_data = (struct muic_ic_data *)sdata->ic_data;
	int vbus_value = 0;
#endif

#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif /* CONFIG_USB_HW_PARAM */

	muic_data->new_dev = ATTACHED_DEV_UNKNOWN_MUIC;

#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	if (gpio_is_valid(muic_data->gpio_dock)) {
		if (gpio_get_value(muic_data->gpio_dock) == 0) {
			usleep_range(35000, 36000);
			MUIC_PDATA_FUNC(ic_data->m_ops.get_vbus_value, ic_data->drv_data, &vbus_value);
			if (vbus_value == 5) {
				muic_data->new_dev = ATTACHED_DEV_POGO_DOCK_5V_MUIC;
				s2mf301_info("[%s:%s] 5V pogo_dock attached\n", MUIC_DEV_NAME, __func__);
			} else if (vbus_value == 9) {
				muic_data->new_dev = ATTACHED_DEV_POGO_DOCK_9V_MUIC;
				s2mf301_info("[%s:%s] 9V pogo_dock attached\n", MUIC_DEV_NAME, __func__);
			} else {
				muic_data->new_dev = ATTACHED_DEV_POGO_DOCK_MUIC;
				s2mf301_info("[%s:%s] pogo_dock attached\n", MUIC_DEV_NAME, __func__);
			}
			goto detect_done;
		}
	}
#endif /* CONFIG_MUIC_HV_SUPPORT_POGO_DOCK */

	/* Attached */
	switch (muic_data->reg[DEVICE_TYPE1]) {
	case S2MF301_MUIC_DEVICE_TYPE1_CDPCHG_MASK:
		if (muic_data->vbvolt) {
			muic_data->new_dev = ATTACHED_DEV_CDP_MUIC;
			s2mf301_info("USB_CDP DETECTED\n");
		}
		break;
	case S2MF301_MUIC_DEVICE_TYPE1_USBCHG_MASK:
		if (muic_data->vbvolt) {
			s2mf301_info("USB DETECTED\n");
			muic_data->new_dev = ATTACHED_DEV_USB_MUIC;
#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
			_s2mf301_muic_set_chg_det(muic_data, MUIC_DISABLE);
#endif
		}
		break;
	case S2MF301_MUIC_DEVICE_TYPE1_DCPCHG_MASK:
	case S2MF301_MUIC_DEVICE_TYPE1_DCPCHG_MASK | S2MF301_MUIC_DEVICE_TYPE1_USBCHG_MASK:
	case S2MF301_MUIC_DEVICE_TYPE1_DCPCHG_MASK | S2MF301_MUIC_DEVICE_TYPE1_CDPCHG_MASK:
		if (muic_data->vbvolt) {
			sdata->is_dcp_charger = true;
			muic_data->new_dev = ATTACHED_DEV_TA_MUIC;
			muic_data->afc_check = true;
			s2mf301_info("DEDICATED CHARGER DETECTED\n");
		}
		break;
	case S2MF301_MUIC_DEVICE_TYPE1_SDP1P8S_MASK:
		if (muic_data->vbvolt) {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
			s2mf301_info("SDP_1P8S DETECTED\n");
			muic_data->new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
#else
			if (muic_data->rescan_cnt >= 1) {
				s2mf301_info("SDP_1P8S DETECTED\n");
				muic_data->new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
				_s2mf301_muic_set_chg_det(muic_data, MUIC_DISABLE);
#endif
				return S2MF301_DETECT_DONE;
			} else {
				schedule_delayed_work(&muic_data->dcd_recheck, msecs_to_jiffies(100));
				return S2MF301_DETECT_SKIP;
			}
		}
		break;
	case S2MF301_MUIC_DEVICE_TYPE1_DP_3V_SDP_MASK:
		if (muic_data->vbvolt) {
			s2mf301_info("DP_3V_SDP DETECTED\n");
			muic_data->new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
			_s2mf301_muic_set_chg_det(muic_data, MUIC_DISABLE);
#endif
#endif
		}
		break;
	case S2MF301_MUIC_DEVICE_TYPE1_OPPO_DCP_MASK:
	case S2MF301_MUIC_DEVICE_TYPE1_U200_CHG_MASK:
		muic_data->new_dev = ATTACHED_DEV_TA_MUIC;
		muic_data->afc_check = false;
		s2mf301_info("CHG_TYPE DETECTED\n");
		if (muic_data->rescan_cnt >= 1) {
			muic_data->new_dev = ATTACHED_DEV_TA_MUIC;
			muic_data->afc_check = false;
		} else {
			schedule_delayed_work(&muic_data->dcd_recheck, msecs_to_jiffies(100));
			return S2MF301_DETECT_SKIP;
		}
		break;
	default:
		break;
	}

	if (muic_data->new_dev != ATTACHED_DEV_UNKNOWN_MUIC &&
			muic_data->new_dev != ATTACHED_DEV_NONE_MUIC)
		goto detect_done;

	if (muic_data->vbvolt &&
		((muic_data->reg[DEVICE_TYPE2] & S2MF301_MUIC_DEVICE_TYPE2_APPLE_2P4A_CHG_MASK)
		|| (muic_data->reg[DEVICE_TYPE2] & S2MF301_MUIC_DEVICE_TYPE2_APPLE_2A_CHG_MASK)
		|| (muic_data->reg[DEVICE_TYPE2] & S2MF301_MUIC_DEVICE_TYPE2_APPLE_1A_CHG_MASK)
		|| (muic_data->reg[DEVICE_TYPE2] & S2MF301_MUIC_DEVICE_TYPE2_APPLE_0P5A_CHG_MASK))) {
		s2mf301_info("APPLE_CHG DETECTED\n");
		muic_data->new_dev = ATTACHED_DEV_TA_MUIC;
		muic_data->afc_check = false;
	}

detect_done:

#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify &&
			muic_data->rescan_cnt >= 1 &&
			muic_data->new_dev != ATTACHED_DEV_TIMEOUT_OPEN_MUIC)
			inc_hw_param(o_notify, USB_MUIC_BC12_RETRY_SUCCESS_COUNT);
#endif /* CONFIG_USB_HW_PARAM */

	if (muic_data->new_dev != ATTACHED_DEV_UNKNOWN_MUIC)
		return S2MF301_DETECT_DONE;
	else
		return S2MF301_DETECT_NONE;
}

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static int s2mf301_muic_mask_rid_change(struct s2mf301_muic_data *muic_data)
{
	int ret = 0;

	s2mf301_info("%s, adc[0x%x]\n", __func__, muic_data->adc);

	switch (muic_data->adc) {
	case ADC_JIG_UART_OFF:
	case ADC_JIG_UART_ON:
	case ADC_JIG_USB_ON:
	case ADC_JIG_USB_OFF:
		TOP_RID_OPS_FUNC_PARAM(mask_rid_change, true);
		PM_RID_OPS_FUNC_PARAM(mask_rid_change, true);
		break;
	default:
		TOP_RID_OPS_FUNC_PARAM(mask_rid_change, false);
		PM_RID_OPS_FUNC_PARAM(mask_rid_change, false);
		break;
	}

	return ret;
}

static int s2mf301_muic_detect_dev_rid_array(struct s2mf301_muic_data *muic_data)
{
	muic_data->new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	switch (muic_data->adc) {
	case ADC_JIG_UART_OFF:
		s2mf301_info("ADC_JIG_UART_OFF\n");
		if (muic_data->vbvolt)
			muic_data->new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
		else
			muic_data->new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		break;
	case ADC_JIG_USB_ON:
	case ADC_DESKDOCK:
		muic_data->new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		s2mf301_info("ADC JIG_USB_ON DETECTED\n");
		break;
	case ADC_JIG_USB_OFF:
		muic_data->new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		s2mf301_info("ADC JIG_USB_OFF DETECTED\n");
		break;
	case ADC_JIG_UART_ON:
		muic_data->new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		s2mf301_info("ADC JIG_UART_ON DETECTED\n");
		break;
	default:
		s2mf301_info("%s unsupported ADC(0x%02x)\n", __func__, muic_data->adc);
	}

	return muic_data->new_dev;
}
#endif /* CONFIG_MUIC_S2MF301_RID */

static void s2mf301_muic_set_water_state(struct s2mf301_muic_data *muic_data, bool en)
{
	struct muic_share_data *sdata = muic_data->sdata;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	int vbvolt = 0;
#endif

	s2mf301_info("%s en : %d\n",	__func__, (int)en);
	if (muic_data->is_water_detected == en) {
		s2mf301_info("%s, skip set water_state\n", __func__);
		return;
	}

	if (en) {
		s2mf301_info("%s: WATER DETECT!!!\n", __func__);
		muic_data->is_water_detected = true;
		sdata->attached_dev = ATTACHED_DEV_UNDEFINED_RANGE_MUIC;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
		vbvolt = _s2mf301_muic_get_vbus_state(muic_data);
		vbus_notifier_handle(vbvolt ? STATUS_VBUS_HIGH : STATUS_VBUS_LOW);
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
		if (vbvolt && !is_lpcharge_pdic_param())
			s2mf301_muic_set_hiccup_mode(muic_data, MUIC_ENABLE);
#endif
#endif /* CONFIG_VBUS_NOTIFIER */
	} else {
		s2mf301_info("%s WATER DRIED!!!\n", __func__);
		muic_data->is_water_detected = false;
		sdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
		s2mf301_muic_set_hiccup_mode(muic_data, MUIC_DISABLE);
#endif
	}
}

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static int s2mf301_muic_set_hiccup_mode(struct s2mf301_muic_data *muic_data, bool en)
{
	s2mf301_info("%s en:%d, lpcharge:%d\n", __func__, (int)en, is_lpcharge_pdic_param());

#if !defined(CONFIG_SEC_FACTORY)
	if (!is_lpcharge_pdic_param()) {
		muic_data->is_hiccup_mode = en;
		_s2mf301_muic_sel_path(muic_data, S2MF301_PATH_OPEN);
	}
#else
	s2mf301_info("%s Do not set hiccup path\n", __func__);
#endif
	return 0;
}

static int s2mf301_muic_set_overheat_hiccup_mode(struct s2mf301_muic_data *muic_data, bool en)
{
	struct muic_share_data *sdata;

	s2mf301_info("%s en:%d, lpcharge:%d\n", __func__, (int)en, is_lpcharge_pdic_param());

	if (muic_data == NULL) {
		s2mf301_info("%s s2mf301 data is null\n", __func__);
		return -ENODEV;
	}

	sdata = muic_data->sdata;

	/* hiccup mode */
	if (_s2mf301_muic_get_vbus_state(muic_data)) {
		if (en == true) {
			muic_data->is_hiccup_mode = en;
			muic_platform_handle_attach(sdata, ATTACHED_DEV_HICCUP_MUIC);
		} else { /* Hiccup mode off */
			_s2mf301_muic_sel_path(muic_data, S2MF301_PATH_OPEN);
		}
	} else /* do not need to change path */ {
		s2mf301_info("%s: Do not need to change path\n", __func__);
	}
	return 0;
}
#endif

static void s2mf301_muic_handle_attached_dev(struct s2mf301_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;

	if (muic_data->new_dev != ATTACHED_DEV_UNKNOWN_MUIC &&
				muic_data->new_dev != sdata->attached_dev) {
		muic_platform_handle_attach(sdata, muic_data->new_dev);
	}
#if IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
	if (muic_data->new_dev == ATTACHED_DEV_UNKNOWN_MUIC &&
				muic_data->new_dev != sdata->attached_dev) {
		muic_platform_handle_detach(sdata);
	}
#endif
}

static void _s2mf301_muic_resend_jig_type(struct s2mf301_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;

	if (muic_data->vbvolt
		&& (sdata->attached_dev == ATTACHED_DEV_JIG_UART_ON_MUIC)) {
		muic_data->new_dev = ATTACHED_DEV_JIG_UART_ON_VB_MUIC;
		s2mf301_muic_handle_attached_dev(muic_data);
	} else if (!muic_data->vbvolt
				&& (sdata->attached_dev == ATTACHED_DEV_JIG_UART_ON_VB_MUIC)) {
		muic_data->new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		s2mf301_muic_handle_attached_dev(muic_data);
	}
}

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
static bool s2mf301_muic_is_opmode_typeC(struct s2mf301_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;
	struct muic_interface_t *muic_if;

	if (sdata->muic_if == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return false;
	}

	muic_if = sdata->muic_if;

	if (muic_if->opmode & OPMODE_PDIC)
		return true;
	else
		return false;
}
#endif

static irqreturn_t s2mf301_muic_attach_isr(int irq, void *data)
{
	struct s2mf301_muic_data *muic_data = data;
	struct muic_share_data *sdata;
	int det_ret = S2MF301_DETECT_NONE;

	if (muic_data == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	sdata = muic_data->sdata;

	if (sdata == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->muic_ws);

	s2mf301_muic_get_detect_info(muic_data);

	if (MUIC_IS_ATTACHED(sdata->attached_dev)) {
		s2mf301_err("%s Cable type already was attached\n", __func__);
		goto attach_skip;
	} else if (muic_data->is_water_detected) {
		s2mf301_err("%s water is detected\n", __func__);
		goto attach_skip;
	}

	s2mf301_info("%s start(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	det_ret = s2mf301_muic_detect_dev_bc1p2(muic_data);
	if (det_ret == S2MF301_DETECT_DONE)
		goto attach_done;
	else if (det_ret == S2MF301_DETECT_SKIP)
		goto attach_skip;

#if 0//IS_ENABLED(CONFIG_MUIC_S2MF301_RID) && IS_ENABLED(CONFIG_MUIC_SUPPORT_TYPEB)
	det_ret = s2mf301_muic_detect_dev_mrid_adc(muic_data);
#endif
attach_done:
	s2mf301_muic_handle_attached_dev(muic_data);

attach_skip:
	s2mf301_info("%s done(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	__pm_relax(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_muic_detach_isr(int irq, void *data)
{
	struct s2mf301_muic_data *muic_data = data;
	struct muic_share_data *sdata;

	if (muic_data == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	sdata = muic_data->sdata;

	if (sdata == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	s2mf301_info("%s start(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->muic_ws);

	s2mf301_muic_get_detect_info(muic_data);
#if IS_ENABLED(CONFIG_S2MF301_IFCONN_HOUSE_NOT_GND)
	if (_s2mf301_muic_get_rescan_status(muic_data)) {
		s2mf301_err("%s rid disable in rescan\n", __func__);
		goto detach_skip;
	}
#endif

	s2mf301_muic_ldo_sel(muic_data, S2MF301_DISABLE);

	if (MUIC_IS_ATTACHED(sdata->attached_dev) == false) {
		s2mf301_err("%s Cable type already was detached\n", __func__);
#if IS_ENABLED(CONFIG_S2MF301_IFCONN_HOUSE_NOT_GND)
		if (muic_data->adc == ADC_OPEN && muic_data->is_cable_inserted) {
			muic_data->is_cable_inserted = false;
			MUIC_SEND_NOTI_TO_PDIC_DETACH(sdata->attached_dev);
		}
#endif
		goto detach_skip;
	}

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PDIC)
	/* TIMEOUT_OPEN is only type that !pdicType && ChgDet(Disable) */
	if (sdata->attached_dev == ATTACHED_DEV_TIMEOUT_OPEN_MUIC
			&& sdata->is_pdic_attached == false) {
#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
		_s2mf301_muic_set_chg_det(muic_data, MUIC_ENABLE);
#endif
	}

	if (s2mf301_muic_is_opmode_typeC(muic_data)) {
		if (!muic_platform_get_pdic_cable_state(sdata)) {
			muic_platform_handle_detach(sdata);
			muic_data->rescan_cnt = 0;
		}
	} else
		muic_platform_handle_detach(sdata);
#else
	muic_platform_handle_detach(sdata);
#endif

detach_skip:
	s2mf301_info("%s done(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	__pm_relax(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_muic_vbus_on_isr(int irq, void *data)
{
	struct s2mf301_muic_data *muic_data = data;
	struct muic_share_data *sdata;
#if defined(CONFIG_S2MF301_TYPEC_WATER)
	struct timespec64 time;
	uint64_t ms;
#endif

	if (muic_data == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	sdata = muic_data->sdata;

	if (sdata == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->muic_ws);

	s2mf301_info("%s start(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	s2mf301_muic_get_detect_info(muic_data);

#if defined(CONFIG_S2MF301_TYPEC_WATER)
	if (!muic_data->is_vbus_on_after_otg) {
		muic_data->is_vbus_on_after_otg = true;
		ktime_get_real_ts64(&time);
		ms = ((time.tv_sec - muic_data->otg_on_time.tv_sec) * 1000) +
			((time.tv_nsec - muic_data->otg_on_time.tv_nsec) / 1000000);
		s2mf301_info("%s, cur(%lld %ld), %llums, vbus(%d)\n", __func__, time.tv_sec,
				time.tv_nsec, ms, _s2mf301_muic_get_vbus_state(muic_data));
		if (ms <= 2000) {
			if (!_s2mf301_muic_get_vbus_state(muic_data)) {
				s2mf301_info("%s, ignored\n", __func__);
				goto out;
			}
		}
	}
#endif

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	if (!is_lpcharge_pdic_param() && muic_data->is_water_detected)
		s2mf301_muic_set_hiccup_mode(muic_data, MUIC_ENABLE);
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_handle(STATUS_VBUS_HIGH);
#endif /* CONFIG_VBUS_NOTIFIER */
	_s2mf301_muic_resend_jig_type(muic_data);

	s2mf301_info("%s done(%s)\n", __func__, dev_to_str(sdata->attached_dev));

#if defined(CONFIG_S2MF301_TYPEC_WATER)
out:
#endif
	s2mf301_muic_vbus_onoff(muic_data);

	__pm_relax(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_muic_vbus_off_isr(int irq, void *data)
{
	struct s2mf301_muic_data *muic_data = data;
	struct muic_share_data *sdata;

	if (muic_data == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	sdata = muic_data->sdata;
	if (sdata == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->muic_ws);

	s2mf301_info("%s start(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	sdata->vbus_state = muic_data->vbvolt = _s2mf301_muic_get_vbus_state(muic_data);
	muic_data->rescan_cnt = 0;

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_handle(STATUS_VBUS_LOW);
#endif /* CONFIG_VBUS_NOTIFIER */
	_s2mf301_muic_resend_jig_type(muic_data);

	if (muic_data->is_timeout_attached) {
		muic_data->is_timeout_attached = false;
		if (sdata->attached_dev == ATTACHED_DEV_TIMEOUT_OPEN_MUIC)
			muic_platform_handle_detach(sdata);
	}

	if (s2mf301_muic_is_opmode_typeC(muic_data)) {
		if (muic_platform_get_pdic_cable_state(sdata)
				&& sdata->is_pdic_attached == false) {
			muic_platform_handle_detach(sdata);
#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
			_s2mf301_muic_set_chg_det(muic_data, MUIC_ENABLE);
#endif
		}
	}

	s2mf301_info("%s done(%s)\n", __func__, dev_to_str(sdata->attached_dev));

	s2mf301_muic_vbus_onoff(muic_data);
	__pm_relax(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static void s2mf301_muic_send_pm_rid_disabled(struct s2mf301_muic_data *muic_data)
{
	struct power_supply *psy_pm;
	union power_supply_propval value;

	psy_pm = power_supply_get_by_name("s2mf301-pmeter");

	value.intval = 0;
	power_supply_set_property(psy_pm, (enum power_supply_property)POWER_SUPPLY_LSI_PROP_RID_DISABLE, &value);
}

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static void s2mf301_muic_get_pm_ops(struct s2mf301_muic_data *muic_data, struct power_supply *psy)
{
	union power_supply_propval value;
	struct s2mf301_pm_rid_ops *p_pm_rid_ops;

	s2mf301_info("%s, get ops success\n", __func__);

	muic_data->rid_isr = s2mf301_muic_rid_isr;
	value.strval = (const char *)muic_data;
	power_supply_get_property(psy, (enum power_supply_property)POWER_SUPPLY_LSI_PROP_RID_OPS, &value);

	muic_data->psy_pm = psy;

	p_pm_rid_ops = (struct s2mf301_pm_rid_ops *)value.strval;

	muic_data->pmeter = p_pm_rid_ops->_data;
	muic_data->pm_rid_ops.rid_is_enable	= p_pm_rid_ops->rid_is_enable;
	muic_data->pm_rid_ops.mask_rid_change	= p_pm_rid_ops->mask_rid_change;
	muic_data->pm_rid_ops.control_rid_adc	= p_pm_rid_ops->control_rid_adc;
	muic_data->pm_rid_ops.get_rid_adc	= p_pm_rid_ops->get_rid_adc;
}

static void s2mf301_muic_get_top_ops(struct s2mf301_muic_data *muic_data, struct power_supply *psy)
{
	union power_supply_propval value;
	struct s2mf301_top_rid_ops *p_top_rid_ops;

	s2mf301_info("%s, get ops success\n", __func__);

	muic_data->rid_isr = s2mf301_muic_rid_isr;
	value.strval = (const char *)muic_data;
	power_supply_get_property(psy, (enum power_supply_property)POWER_SUPPLY_LSI_PROP_RID_OPS, &value);

	muic_data->psy_top = psy;

	p_top_rid_ops = (struct s2mf301_top_rid_ops *)value.strval;

	muic_data->top = p_top_rid_ops->_data;
	muic_data->top_rid_ops.set_jig_on = p_top_rid_ops->set_jig_on;
	muic_data->top_rid_ops.mask_rid_change = p_top_rid_ops->mask_rid_change;
}

static void s2mf301_muic_get_pm_ops_work(struct work_struct *work)
{
	struct s2mf301_muic_data *muic_data =
	    container_of(work, struct s2mf301_muic_data, get_pm_ops_work.work);
	struct power_supply *psy_pm;

	psy_pm = power_supply_get_by_name("s2mf301-pmeter");

	if (psy_pm) {
		s2mf301_muic_get_pm_ops(muic_data, psy_pm);
	} else {
		s2mf301_info("%s, pmeter is not probed, retry\n", __func__);
		schedule_delayed_work(&muic_data->get_pm_ops_work, msecs_to_jiffies(100));
	}
}

static void s2mf301_muic_get_top_ops_work(struct work_struct *work)
{
	struct s2mf301_muic_data *muic_data =
	    container_of(work, struct s2mf301_muic_data, get_top_ops_work.work);
	struct power_supply *psy_top;

	psy_top = power_supply_get_by_name("s2mf301-top");

	if (psy_top) {
		s2mf301_muic_get_top_ops(muic_data, psy_top);
	} else {
		s2mf301_info("%s, top is not probed, retry\n", __func__);
		schedule_delayed_work(&muic_data->get_top_ops_work, msecs_to_jiffies(100));
	}
}

static void s2mf301_muic_init_rid_ops(struct s2mf301_muic_data *muic_data)
{
	struct power_supply *psy_pm;
	struct power_supply *psy_top;

	psy_pm = power_supply_get_by_name("s2mf301-pmeter");

	if (psy_pm) {
		s2mf301_muic_get_pm_ops(muic_data, psy_pm);
	} else {
		s2mf301_info("%s, pmeter is not probed, retry\n", __func__);
		INIT_DELAYED_WORK(&muic_data->get_pm_ops_work, s2mf301_muic_get_pm_ops_work);
		schedule_delayed_work(&muic_data->get_pm_ops_work, msecs_to_jiffies(100));
	}

	psy_top = power_supply_get_by_name("s2mf301-top");
	if (psy_top) {
		s2mf301_muic_get_top_ops(muic_data, psy_top);
	} else {
		s2mf301_info("%s, top is not probed, retry\n", __func__);
		INIT_DELAYED_WORK(&muic_data->get_top_ops_work, s2mf301_muic_get_top_ops_work);
		schedule_delayed_work(&muic_data->get_top_ops_work, msecs_to_jiffies(100));
	}
}

static int s2mf301_muic_rid_isr(void *_data)
{
	struct s2mf301_muic_data *muic_data = _data;
	struct muic_share_data *sdata;
	struct muic_interface_t *muic_if;

	if (muic_data == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	sdata = muic_data->sdata;

	if (sdata == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	muic_if = sdata->muic_if;

	if (muic_if == NULL) {
		s2mf301_err("%s data NULL\n", __func__);
		return IRQ_NONE;
	}

	mutex_lock(&muic_data->muic_mutex);
	__pm_stay_awake(muic_data->muic_ws);
	
	s2mf301_info("%s, wait adc check time 300ms\n", __func__);
	msleep(300);

	muic_data->adc = _s2mf301_muic_get_rid_adc(muic_data);
	muic_data->vbvolt = sdata->vbus_state = _s2mf301_muic_get_vbus_state(muic_data);

	s2mf301_info("%s Vbus(%s), rid_adc(%#x), Type(%s), opmode : %s\n", __func__,
			(muic_data->vbvolt ? "High" : "Low"),
			muic_data->adc, dev_to_str(sdata->attached_dev),
			(muic_if->opmode ? "PDIC" : "MUIC"));

	if (muic_if->opmode == OPMODE_MUIC) {
		s2mf301_muic_mask_rid_change(muic_data);
		s2mf301_muic_detect_dev_rid_array(muic_data);
		s2mf301_muic_handle_attached_dev(muic_data);
	}
	__pm_relax(muic_data->muic_ws);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t s2mf301_muic_reserved_isr(int irq, void *data)
{
	s2mf301_info("%s irq_num(%d)\n", __func__, irq);

	return IRQ_HANDLED;
}

static int s2mf301_muic_irq_init(struct s2mf301_muic_data *muic_data)
{
	int ret = 0;

	s2mf301_info("%s+\n", __func__);
	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		/* request MUIC IRQ */
		muic_data->irq_attach = irq_base + S2MF301_MUIC_IRQ_ATTATCH;
		REQUEST_IRQ(muic_data->irq_attach, muic_data,
			"muic-attach", &s2mf301_muic_attach_isr);

		muic_data->irq_detach = irq_base + S2MF301_MUIC_IRQ_DETACH;
		REQUEST_IRQ(muic_data->irq_detach, muic_data,
			"muic-detach", &s2mf301_muic_detach_isr);

		muic_data->irq_vbus_on = irq_base + S2MF301_MUIC_IRQ_VBUS_ON;
		REQUEST_IRQ(muic_data->irq_vbus_on, muic_data,
			"muic-vbus_on", &s2mf301_muic_vbus_on_isr);

		muic_data->irq_rsvd_attach = irq_base + S2MF301_MUIC_IRQ_RESERVED;
		REQUEST_IRQ(muic_data->irq_rsvd_attach, muic_data,
			"muic-rsvd_attach", &s2mf301_muic_reserved_isr);

		muic_data->irq_vbus_off = irq_base + S2MF301_MUIC_IRQ_VBUS_OFF;
		REQUEST_IRQ(muic_data->irq_vbus_off, muic_data,
			"muic-vbus_off", &s2mf301_muic_vbus_off_isr);
	}

	s2mf301_info("%s muic-attach(%d), muic-detach(%d), muic-rid_chg(%d), muic-vbus_on(%d)",
		__func__, muic_data->irq_attach, muic_data->irq_detach, muic_data->irq_rid_chg,
			muic_data->irq_vbus_on);
	s2mf301_info("muic-rsvd_attach(%d), muic-adc_change(%d), muic-av_charge(%d), muic-vbus_off(%d)\n",
		muic_data->irq_rsvd_attach, muic_data->irq_adc_change,
		muic_data->irq_av_charge, muic_data->irq_vbus_off);

	return ret;
}

static void s2mf301_muic_free_irqs(struct s2mf301_muic_data *muic_data)
{
	s2mf301_info("%s\n", __func__);

	/* free MUIC IRQ */
	FREE_IRQ(muic_data->irq_attach, muic_data, "muic-attach");
	FREE_IRQ(muic_data->irq_detach, muic_data, "muic-detach");
	FREE_IRQ(muic_data->irq_vbus_on, muic_data, "muic-vbus_on");
	FREE_IRQ(muic_data->irq_rsvd_attach, muic_data, "muic-rsvd_attach");
	FREE_IRQ(muic_data->irq_av_charge, muic_data, "muic-av_charge");
	FREE_IRQ(muic_data->irq_vbus_off, muic_data, "muic-vbus_off");
}

#if IS_ENABLED(CONFIG_OF)
static int of_s2mf301_muic_dt(struct device *dev,
			      struct s2mf301_muic_data *muic_data)
{
	struct muic_share_data *sdata = muic_data->sdata;
	struct device_node *np, *np_muic;
	int ret = 0;
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	struct device_node *np_pdic;
	int pogo_gpio;
#endif

	np = dev->parent->of_node;
	if (!np) {
		s2mf301_err("%s : could not find np\n", __func__);
		return -ENODEV;
	}

	np_muic = of_find_node_by_name(np, "muic");
	if (!np_muic) {
		s2mf301_err("%s : could not find muic sub-node np_muic\n", __func__);
		return -EINVAL;
	} else {
		muic_data->is_sido_vbus_switch = of_property_read_bool(np_muic, "uses_sido_vbus_switch");
		if (muic_data->is_sido_vbus_switch)
			s2mf301_info("%s, using sido vbus switch\n", __func__);
		else
			s2mf301_info("%s, using siso vbus switch\n", __func__);

		muic_data->is_disable_afc = of_property_read_bool(np_muic, "disable_afc");
		if (muic_data->is_disable_afc)
			s2mf301_info("%s, disable afc charging\n", __func__);
		else
			s2mf301_info("%s, enable afc charging\n", __func__);
	}
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	np_pdic = of_find_node_by_name(np, "usbpd-s2mf301");
	if (!np_pdic) {
		s2mf301_err("%s : could not find pdic sub-node np_pdic\n", __func__);
		return -EINVAL;
	} else {
		pogo_gpio = of_get_named_gpio(np_pdic, "pogo_dock_int", 0);
		muic_data->gpio_dock = pogo_gpio;
		if (gpio_is_valid(muic_data->gpio_dock))
			s2mf301_info("%s:%s pogo_dock_int:%d, value:%d\n", MUIC_DEV_NAME, __func__,
				muic_data->gpio_dock, gpio_get_value(muic_data->gpio_dock));
		else
			s2mf301_err("%s:%s pogo_dock_int is invalid\n", MUIC_DEV_NAME, __func__);

		pogo_gpio = gpio_to_irq(pogo_gpio);
		if (pogo_gpio < 0) {
			s2mf301_err("%s: fail to return irq corresponding gpio\n", __func__);
			return -EINVAL;
		}
		muic_data->irq_dock = pogo_gpio;
	}
#endif /* CONFIG_MUIC_HV_SUPPORT_POGO_DOCK */

#if !IS_ENABLED(CONFIG_MUIC_UART_SWITCH)
	if (of_gpio_count(np_muic) < 1) {
		s2mf301_err("%s : could not find muic gpio\n", __func__);
		sdata->gpio_uart_sel = -1;
	} else
		sdata->gpio_uart_sel = of_get_gpio(np_muic, 0);
#else
	muic_data->pdata->uart_addr =
	    (const char *)of_get_property(np_muic, "muic,uart_addr", NULL);
	muic_data->pdata->uart_txd =
	    (const char *)of_get_property(np_muic, "muic,uart_txd", NULL);
	muic_data->pdata->uart_rxd =
	    (const char *)of_get_property(np_muic, "muic,uart_rxd", NULL);
#endif

	return ret;
}
#endif /* CONFIG_OF */

static void s2mf301_muic_init_drvdata(struct s2mf301_muic_data *muic_data,
				      struct s2mf301_dev *s2mf301,
				      struct platform_device *pdev,
				      struct s2mf301_platform_data *mfd_pdata)
{
	/* save platfom data for gpio control functions */
	muic_data->s2mf301_dev = s2mf301;
	muic_data->dev = &pdev->dev;
	muic_data->i2c = s2mf301->muic;
	muic_data->mfd_pdata = mfd_pdata;
	muic_data->afc_check = false;
	muic_data->rescan_cnt = 0;
	muic_data->is_timeout_attached = false;
#if IS_ENABLED(CONFIG_S2MF301_IFCONN_HOUSE_NOT_GND)
	muic_data->is_rescanning = false;
#endif
	muic_data->is_water_detected = false;
#if defined(CONFIG_S2MF301_TYPEC_WATER)
	muic_data->is_vbus_on_after_otg = true;
#endif
}

void s2mf301_muic_charger_init(void)
{
	struct s2mf301_muic_data *muic_data = static_data;
	int vbvolt = 0;

	s2mf301_info("%s\n", __func__);

	if (muic_data == NULL) {
		s2mf301_info("%s, muic_data is NULL\n", __func__);
		return;
	}

	vbvolt = _s2mf301_muic_get_vbus_state(muic_data);

	if (muic_data->is_water_detected == true) {
		s2mf301_info("%s, release hiccup path for re-recognization.\n", __func__);
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
		if (!vbvolt)
			s2mf301_muic_set_hiccup_mode(muic_data, 0);
#endif
	}
}
EXPORT_SYMBOL(s2mf301_muic_charger_init);

static void register_muic_ops(struct muic_ic_data *ic_data)
{
#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
	ic_data->m_ops.get_adc = s2mf301_ops_get_adc;
#endif
	ic_data->m_ops.get_vbus_state = s2mf301_ops_get_vbus_state;
	ic_data->m_ops.get_sbu_ovp_state = s2mf301_ops_get_sbu_ovp_state;
	ic_data->m_ops.set_gpio_uart_sel = s2mf301_ops_set_gpio_uart_sel;
	ic_data->m_ops.show_register = s2mf301_ops_show_register;
	ic_data->m_ops.set_switch_to_open = s2mf301_ops_com_to_open;
	ic_data->m_ops.set_switch_to_uart = s2mf301_ops_com_to_uart;
	ic_data->m_ops.set_switch_to_usb = s2mf301_ops_com_to_usb;
	ic_data->m_ops.set_chg_det = s2mf301_ops_set_chg_det;
	ic_data->m_ops.set_bypass = s2mf301_ops_set_bypass;
	ic_data->m_ops.set_water_state = s2mf301_ops_set_water_state;
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	ic_data->m_ops.check_usb_killer = s2mf301_ops_check_usb_killer;
#endif
	ic_data->m_ops.set_jig_ctrl_on = s2mf301_ops_set_jig_ctrl_on;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	ic_data->m_ops.set_hiccup_mode = s2mf301_ops_set_overheat_hiccup_mode;
	ic_data->m_ops.get_hiccup_mode = s2mf301_ops_get_hiccup_mode;
	ic_data->m_ops.set_hiccup = s2mf301_ops_set_hiccup_mode;
#endif
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_PRSWAP)
	ic_data->m_ops.prswap_work = s2mf301_ops_prswap_work;
#endif
}

static int s2mf301_muic_probe(struct platform_device *pdev)
{
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_platform_data *mfd_pdata = dev_get_platdata(s2mf301->dev);
	struct s2mf301_muic_data *muic_data;
	struct muic_interface_t *muic_if;
	struct muic_share_data *sdata;
	struct muic_ic_data *ic_data;
	int ret = 0;
	u8 adc = 0;

	s2mf301_info("%s start\n", __func__);
	muic_data = devm_kzalloc(&pdev->dev, sizeof(*muic_data), GFP_KERNEL);
	if (unlikely(!muic_data)) {
		s2mf301_err("%s out of memory\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	if (unlikely(!mfd_pdata)) {
		s2mf301_err("%s failed to get s2mf301 mfd platform data\n",
		       __func__);
		ret = -ENOMEM;
		goto err_kfree1;
	}
	sdata = kzalloc(sizeof(*sdata), GFP_KERNEL);
	if (unlikely(!sdata)) {
		s2mf301_err("%s out of memory\n", __func__);
		goto err_kfree1;
	}
	muic_data->sdata = sdata;
	ic_data = kzalloc(sizeof(*ic_data), GFP_KERNEL);
	if (unlikely(!ic_data)) {
		s2mf301_err("%s out of memory\n", __func__);
		goto err_kfree2;
	}

	register_muic_ops(ic_data);
	ic_data->drv_data = (void *) muic_data;
	sdata->ic_data = ic_data;

	ret = register_muic_platform_layer(sdata);
	if (ret < 0) {
		s2mf301_err("%s fail to register muic platform layer\n", __func__);
		goto err_kfree3;
	}

	muic_if = sdata->muic_if;
	static_data = muic_data;

	s2mf301_muic_init_drvdata(muic_data, s2mf301, pdev, mfd_pdata);

#if IS_ENABLED(CONFIG_OF)
	ret = of_s2mf301_muic_dt(&pdev->dev, muic_data);
	if (ret < 0)
		s2mf301_err("no muic dt! ret[%d]\n", ret);
#endif /* CONFIG_OF */

	mutex_init(&muic_data->muic_mutex);
	mutex_init(&muic_data->switch_mutex);
	mutex_init(&muic_data->bcd_rescan_mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 188)
	wakeup_source_init(muic_data->muic_ws, "muic_wake");   // 4.19 R
	if (!(muic_data->muic_ws)) {
		muic_data->muic_ws = wakeup_source_create("muic_wake"); // 4.19 Q
		if (muic_data->muic_ws)
			wakeup_source_add(muic_data->muic_ws);
	}
#else
	muic_data->muic_ws = wakeup_source_register(NULL, "muic_wake"); // 5.4 R
#endif

	platform_set_drvdata(pdev, muic_data);

	muic_platform_init_gpio_cb(sdata);
	muic_platform_init_switch_dev(sdata);

#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	ret = s2mf301_hv_muic_init(muic_data);
	if (ret) {
		s2mf301_err("failed to init hv-muic(%d)\n", ret);
		goto fail;
	}
#endif /* CONFIG_HV_MUIC_S2MF301_AFC */

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
	s2mf301_muic_init_rid_ops(muic_data);
#endif

	ret = s2mf301_muic_reg_init(muic_data);
	if (ret) {
		s2mf301_err("failed to init muic(%d)\n", ret);
		goto fail;
	}

	if (sdata->is_rustproof) {
		s2mf301_err("%s rustproof is enabled\n", __func__);
		ret = _s2mf301_muic_sel_path(muic_data, S2MF301_PATH_OPEN);
	}
	INIT_DELAYED_WORK(&muic_data->dcd_recheck, s2mf301_muic_dcd_recheck);
	INIT_DELAYED_WORK(&muic_data->pcp_clk_work, s2mf301_muic_pcp_clk_work);
	INIT_DELAYED_WORK(&muic_data->rescan_validity_checker,
		s2mf301_muic_rescan_validity_checker);
#if defined(CONFIG_MUIC_SUPPORT_POWERMETER)
	ret = muic_platform_psy_init(sdata, &pdev->dev);
	if (ret)
		s2mf301_err("%s failed to init psy(%d)\n", __func__, ret);
#endif

	ret = s2mf301_muic_irq_init(muic_data);
	if (ret) {
		s2mf301_err("%s failed to init irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}
#if defined(CONFIG_MUIC_HV_SUPPORT_POGO_DOCK)
	ret = request_threaded_irq(muic_data->irq_dock, NULL, s2mf301_muic_attach_isr,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"pogo_dock", muic_data);
	if (ret < 0) {
		s2mf301_err("[%s:%s] failed to request_threaded_irq pogo\n", MUIC_DEV_NAME, __func__);
		goto fail;
	}
#endif
	adc = _s2mf301_muic_get_rid_adc(muic_data);
	s2mf301_info("%s rid adc=%02x\n", __func__, adc);
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	if (IS_JIG_ADC(adc))
		muic_if->opmode = OPMODE_MUIC;
	else
		_s2mf301_muic_control_rid_adc(muic_data, false);
#else
	_s2mf301_muic_control_rid_adc(muic_data, false);
	if (adc == ADC_GND) {
		s2mf301_muic_bcd_rescan(muic_data);
		msleep(500);
	}
#endif

	s2mf301_info("%s muic_if->opmode(%d)\n", __func__, muic_if->opmode);

	if (muic_if->opmode == OPMODE_MUIC) {
#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
		s2mf301_muic_rid_isr(muic_data);
#endif
	} else {
		s2mf301_muic_send_pm_rid_disabled(muic_data);
		s2mf301_muic_get_detect_info(muic_data);
		s2mf301_muic_attach_isr(-1, muic_data);
	}

	s2mf301_info("%s done\n", __func__);
	return 0;

fail_init_irq:
fail:
	mutex_destroy(&muic_data->bcd_rescan_mutex);
	mutex_destroy(&muic_data->switch_mutex);
	mutex_destroy(&muic_data->muic_mutex);
err_kfree3:
	kfree(ic_data);
err_kfree2:
	kfree(sdata);
err_kfree1:
err_return:
	return ret;
}

/* if need to set s2mf301 pdata */
static const struct of_device_id s2mf301_muic_match_table[] = {
	{.compatible = "samsung,s2mf301-muic",},
	{},
};

static int s2mf301_muic_remove(struct platform_device *pdev)
{
	struct s2mf301_muic_data *muic_data = platform_get_drvdata(pdev);

	if (muic_data) {
		s2mf301_info("%s\n", __func__);
		muic_platform_deinit_switch_dev(muic_data->sdata);
		disable_irq_wake(muic_data->i2c->irq);
		s2mf301_muic_free_irqs(muic_data);
		mutex_destroy(&muic_data->muic_mutex);
		mutex_destroy(&muic_data->switch_mutex);
		i2c_set_clientdata(muic_data->i2c, NULL);
		unregister_muic_platform_layer(muic_data->sdata);
		kfree(muic_data->sdata->ic_data);
		kfree(muic_data->sdata);
	}

	return 0;
}

static void s2mf301_muic_shutdown(struct platform_device *pdev)
{
	struct s2mf301_muic_data *muic_data = platform_get_drvdata(pdev);
	int ret;
	u8 reg;

	s2mf301_info("%s\n", __func__);

	if (!muic_data->i2c) {
		s2mf301_err("%s no muic i2c client\n", __func__);
		return;
	}
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	s2mf301_hv_muic_remove(muic_data);
#endif /* CONFIG_HV_MUIC_S2MF301_AFC */

	reg = s2mf301_i2c_read_byte(muic_data->i2c, S2MF301_REG_MANUAL_SW_CTRL);
	reg &= ~S2MF301_MANUAL_SW_CTRL_SWITCH_MASK;
	s2mf301_i2c_write_byte(muic_data->i2c, S2MF301_REG_MANUAL_SW_CTRL, reg);

	/* set auto sw mode before shutdown to make sure device goes into */
	/* LPM charging when TA or USB is connected during off state */
	ret = _s2mf301_muic_set_path_mode(muic_data, S2MF301_MODE_AUTO);
	if (ret < 0) {
		s2mf301_err("%s fail to update reg\n", __func__);
		return;
	}
#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
	_s2mf301_muic_set_chg_det(muic_data, MUIC_ENABLE);
#endif
}

#if IS_ENABLED(CONFIG_PM)
static int s2mf301_muic_suspend(struct device *dev)
{
	struct s2mf301_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_share_data *sdata = muic_data->sdata;

	sdata->suspended = true;

	return 0;
}

static int s2mf301_muic_resume(struct device *dev)
{
	struct s2mf301_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_share_data *sdata = muic_data->sdata;

	sdata->suspended = false;

	if (sdata->need_to_noti) {
		if (sdata->attached_dev)
			MUIC_SEND_NOTI_ATTACH(sdata->attached_dev);
		else
			MUIC_SEND_NOTI_DETACH(sdata->attached_dev);

		sdata->need_to_noti = false;
	}

	return 0;
}
#else
#define s2mf301_muic_suspend NULL
#define s2mf301_muic_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mf301_muic_pm_ops, s2mf301_muic_suspend,
			 s2mf301_muic_resume);

static struct platform_driver s2mf301_muic_driver = {
	.probe = s2mf301_muic_probe,
	.remove = s2mf301_muic_remove,
	.shutdown = s2mf301_muic_shutdown,
	.driver = {
		   .name = "s2mf301-muic",
		   .owner = THIS_MODULE,
		   .of_match_table = s2mf301_muic_match_table,
#if IS_ENABLED(CONFIG_PM)
		   .pm = &s2mf301_muic_pm_ops,
#endif
		   },
};

static int __init s2mf301_muic_init(void)
{
	return platform_driver_register(&s2mf301_muic_driver);
}

module_init(s2mf301_muic_init);

static void __exit s2mf301_muic_exit(void)
{
	platform_driver_unregister(&s2mf301_muic_driver);
}

module_exit(s2mf301_muic_exit);

MODULE_DESCRIPTION("Samsung S2MF301 Micro USB IC driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: s2mf301_pmeter top_s2mf301");
MODULE_SOFTDEP("post: s2mf301-usbpd");
