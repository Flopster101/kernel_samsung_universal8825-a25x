/*
 driver/usbpd/s2mf301.c - S2MF301 USB PD(Power Delivery) device driver
 *
 * Copyright (C) 2016 Samsung Electronics
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
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/version.h>

#include <linux/usb/typec/slsi/common/usbpd.h>
#include <linux/usb/typec/slsi/s2mf301/usbpd-s2mf301.h>
#include <linux/usb/typec/common/pdic_sysfs.h>
#include <linux/usb/typec/common/pdic_param.h>

#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include <linux/mfd/slsi/s2mf301/s2mf301_log.h>

#include <linux/muic/common/muic.h>
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && !defined(CONFIG_BATTERY_GKI)
#include <linux/sec_batt.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#else
#include <linux/battery/sec_pd.h>
#endif
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#if IS_ENABLED(CONFIG_PM_S2MF301)
#include "../../../../battery/charger/s2mf301_charger/s2mf301_pmeter.h"
#else
#include "../../../../battery/common/sec_charging_common.h"
#endif
#endif

#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY) || IS_ENABLED(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif
#include <linux/regulator/consumer.h>

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) || IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/typec/slsi/common/usbpd_ext.h>
#endif

/*
*VARIABLE DEFINITION
*/
static usbpd_phy_ops_type s2mf301_ops;

static int slice_mv[] = {
	0,      0,      127,    171,    214,    //0~4
	257,    300,    342,    385,    428,    //5~9
	450,    471,    492,    514,    535,    //10~14
	557,    578,    600,    621,    642,    //15~19
	685,    0,      0,      814,    0,      //20~24
	0,      0,      1000,   0,      0,      //25~29
	0,      0,      1200,   1242,   1285,   //30~34
	1328,   1371,   1414,   1457,   1500,   //35~39
	1542,   1587,   1682,   1671,   1714,   //40~44
	1757,   1799,   1842,   1885,   1928,   //45~49
	1971,   2014,   2057,   2099,   2142,   //50~54
	2185,   2228,   2271,   2666,   2666,   //55~59
	2666,   2666,   2666,   2666    //60~63
};

extern int S2MF301_PM_RWATER;
extern int S2MF301_PM_VWATER;
extern int S2MF301_PM_RDRY;
extern int S2MF301_PM_VDRY;
extern int S2MF301_PM_DRY_TIMER_SEC;
extern int S2MF301_PM_WATER_CHK_DELAY_MSEC;

/*
*FUNCTION DEFINITION
*/
static int s2mf301_receive_message(void *data);
static int s2mf301_check_port_detect(struct s2mf301_usbpd_data *pdic_data);
static int s2mf301_usbpd_reg_init(struct s2mf301_usbpd_data *_data);
static void s2mf301_dfp(struct i2c_client *i2c);
static void s2mf301_ufp(struct i2c_client *i2c);
static void s2mf301_src(struct i2c_client *i2c);
static void s2mf301_snk(struct i2c_client *i2c);
static void s2mf301_assert_rd(void *_data);
static void s2mf301_assert_rp(void *_data);
static void s2mf301_assert_drp(void *_data);
static void s2mf301_usbpd_check_rid(struct s2mf301_usbpd_data *pdic_data);
static int s2mf301_usbpd_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
static int s2mf301_usbpd_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
static void s2mf301_usbpd_notify_detach(struct s2mf301_usbpd_data *pdic_data);
static void s2mf301_usbpd_detach_init(struct s2mf301_usbpd_data *pdic_data);
static int s2mf301_usbpd_set_pd_control(struct s2mf301_usbpd_data  *pdic_data, int val);
static void s2mf301_usbpd_set_rp_scr_sel(struct s2mf301_usbpd_data *pdic_data,
													PDIC_RP_SCR_SEL scr_sel);
int s2mf301_usbpd_check_msg(void *_data, u64 *val);
static int s2mf301_usbpd_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
static void s2mf301_vbus_short_check(struct s2mf301_usbpd_data *pdic_data);
static void s2mf301_self_soft_reset(struct s2mf301_usbpd_data *pdic_data);
static void s2mf301_usbpd_set_vbus_wakeup(struct s2mf301_usbpd_data *pdic_data, PDIC_VBUS_WAKEUP_SEL sel);
void s2mf301_usbpd_set_cc_state(struct s2mf301_usbpd_data *pdic_data, int cc);
static void s2mf301_set_irq_enable(struct s2mf301_usbpd_data *_data, u8, u8, u8, u8, u8, u8);
static void s2mf301_set_revision(void *_data, int val);

static char *s2m_cc_state_str[] = {
	"CC_OPEN",
	"CC_RD",
	"CC_DRP",
	"CC_DEFAULT",
};

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
static int s2mf301_power_off_water_check(struct s2mf301_usbpd_data *pdic_data);
static void s2mf301_power_off_water_notify(struct s2mf301_usbpd_data *pdic_data);
const char *WATER_STATUS_TO_STR[] = {
	"INVALID",
	"DRY",
	"WATER",
};
#endif

#define SET_STATUS(status, shift) \
	status[shift > 63 ? 1:0] |= 1ULL << (shift & 63)

static void s2mf301_usbpd_test_read(struct s2mf301_usbpd_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	static int reg_list[] = {0x01, 0x18, 0x1b, 0x27, 0x28, 0x2e,
		0x40, 0xe2, 0xb2, 0xb3, 0xb4, 0xf7};
	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
		s2mf301_usbpd_read_reg(i2c, reg_list[i], &data);
		sprintf(str+strlen(str), "0x%02x[0x%02x], ", reg_list[i], data);
	}

	/* print buffer */
	s2mf301_info("[PD]%s: %s\n", __func__, str);
}

#if IS_ENABLED(CONFIG_S2MF301_PDIC_SUPPORT_S2MC501)
static int s2mf301_pps_enable(void *_data, int val)
{
    struct usbpd_data *pd_data = _data;
    struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
    struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_senden, reg_opmode, reg_pps;
	int max_cur = pd_data->pd_noti.sink_status.power_list[pd_data->pd_noti.sink_status.selected_pdo_num].max_current;

	s2mf301_info("%s, en(%d), max_cur(%d)\n", __func__, val, max_cur);
    s2mf301_usbpd_read_reg(i2c, S2MF301_REG_MSG_SEND_CON, &reg_opmode);
    s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, &reg_senden);
    s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PPS_CTRL, &reg_pps);

	if (val) {
		reg_opmode &= ~S2MF301_REG_MSG_SEND_CON_OP_MODE;
		reg_senden |= S2MF301_REG_SEND_EN_CLEAR_SEL;
		reg_pps |= S2MF301_REG_PPS_ENABLE_MASK;
		reg_pps &= ~S2MF301_REG_PPS_TIMER_MASK;
		reg_pps |= ~S2MF301_REG_PPS_TIMER_8S_MASK;
	    s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PPS_MIN_CUR, (1000 / 50));
	    s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PPS_MAX_CUR, (max_cur / 50));
		s2mf301_set_irq_enable(pdic_data, 0, ENABLED_INT_1_PPS,
				0, 0, ENABLED_INT_4_PPS, ENABLED_INT_5);
	} else {
		reg_opmode |= S2MF301_REG_MSG_SEND_CON_OP_MODE;
		reg_senden &= ~S2MF301_REG_SEND_EN_CLEAR_SEL;
		reg_pps &= ~S2MF301_REG_PPS_ENABLE_MASK;
		reg_pps &= ~S2MF301_REG_PPS_TIMER_MASK;
		s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
	}
	pdic_data->pps_enable = val;

    s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, reg_senden);
    s2mf301_usbpd_write_reg(i2c, S2MF301_REG_MSG_SEND_CON, reg_opmode);
    s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PPS_CTRL, reg_pps);

	return 0;
}

static int s2mf301_get_pps_enable(void *_data, int *val)
{
    struct usbpd_data *pd_data = _data;
    struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	*val = pdic_data->pps_enable;

	return 0;
}
#endif

int s2mf301_usbpd_get_rev(struct s2mf301_usbpd_data *pdic_data)
{
	struct power_supply *psy_top;
	union power_supply_propval val;
	int ret = 0;

	if (pdic_data->chip_rev < 0) {
		psy_top = get_power_supply_by_name("s2mf301-top");
		if (!psy_top) {
			s2mf301_info("%s, Fail to get psy_top\n", __func__);
			return -EINVAL;
		}
		ret = psy_top->desc->get_property(psy_top,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_GET_REV, &val);
		if (ret) {
			s2mf301_info("%s, Fail to get psy_top prop, ret(%d)\n", __func__, ret);
			return -EINVAL;
		}

		pdic_data->chip_rev = val.intval;
		s2mf301_info("%s, CHIP REV = 0x%x\n", __func__, pdic_data->chip_rev);
	}

	return pdic_data->chip_rev;
}

void s2mf301_usbpd_check_pps_irq(void *_data, int enable)
{

	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg;

	s2mf301_info("%s, enable(%d)\n", __func__, enable);
	pdic_data->check_pps_irq = enable;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PPS_CTRL, &reg);
	if (enable)
		reg &= ~S2MF301_REG_PPS_CTRL_MSG_IRQ_SEL;
	else
		reg |= S2MF301_REG_PPS_CTRL_MSG_IRQ_SEL;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PPS_CTRL, reg);
}

void s2mf301_usbpd_check_pps_irq_tx_req(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	s2mf301_info("%s,\n", __func__);
	pdic_data->check_pps_irq_msg = 0;
	pdic_data->check_pps_irq_pass = 0;

	ktime_get_real_ts64(&pdic_data->time_tx);
}

long long s2mf301_usbpd_check_pps_timer(struct timespec64 *time_tx)
{
	uint32_t ms = 0;
	uint32_t sec = 0;
	struct timespec64 time;

	ktime_get_real_ts64(&time);

	sec = time.tv_sec - time_tx->tv_sec;
	ms = (uint32_t)((time.tv_nsec - time_tx->tv_nsec) / 1000000);

	return (sec * 1000) + ms;
}

void s2mf301_usbpd_check_pps_irq_reduce_clk(void *_data, int reduce)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct policy_data *policy = &pd_data->policy;
	u8 saved_data = 0, otp_0a = 0;
	long long ms = 0;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_DUMMY, &saved_data);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, &otp_0a);
	ms = s2mf301_usbpd_check_pps_timer(&pdic_data->time_tx);

	s2mf301_info("%s, reduce(%d), OTP(0x%x), saved(0x%x), %lld ms\n", __func__,
			reduce, otp_0a, saved_data, ms);

	/* normal PPS -> TxReq ~ RxPsReady <= 30ms */
	if ((!reduce) || (pdic_data->osc_clk == S2MF301_OSC_CLK_250) || (ms > 180)) {
		pdic_data->osc_clk = S2MF301_OSC_CLK_DEFAULT;

		otp_0a &= ~(0x7F);
		otp_0a |= (saved_data & 0x7F);
		if (!reduce)
			policy->need_check_pps_clk = 0;
	} else {
		s2mf301_info("%s, osc_clk(%d), msgIRQ(%d), passIRQ(%d)\n", __func__,
				pdic_data->osc_clk, pdic_data->check_pps_irq_msg,
				pdic_data->check_pps_irq_pass);
		if (pdic_data->check_pps_irq_msg != pdic_data->check_pps_irq_pass) {
			switch (pdic_data->osc_clk) {
			case S2MF301_OSC_CLK_DEFAULT:
				pdic_data->osc_clk = S2MF301_OSC_CLK_270;
				otp_0a &= ~(0x7F);
				otp_0a |= (saved_data & 0x7F) - pdic_data->clk_offset;
				break;
			case S2MF301_OSC_CLK_270:
				pdic_data->osc_clk = S2MF301_OSC_CLK_250;
				otp_0a &= ~(0x7F);
				otp_0a |= (saved_data & 0x7F) -
					(pdic_data->clk_offset + pdic_data->clk_offset2);
				break;
			case S2MF301_OSC_CLK_250:
				pdic_data->osc_clk = S2MF301_OSC_CLK_DEFAULT;
				otp_0a &= ~(0x7F);
				otp_0a |= (saved_data & 0x7F);
				break;
			}
		} else
			s2mf301_info("%s, pps & reset, but no PD err\n", __func__);
	}

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, otp_0a);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, &otp_0a);
	s2mf301_info("%s, new otp(0x%x, %d)\n", __func__, otp_0a, otp_0a & 0x7F);
}

void s2mf301_usbpd_set_clk_offset(void *_data, int offset)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	int offset1 = offset & 0xff;
	int offset2 = (offset >> 8) & 0xff;

	pdic_data->clk_offset = offset1;
	pdic_data->clk_offset2 = offset2;
	s2mf301_info("%s, offset(0x%x), CLK OFFSET (%d, %d)\n", __func__,
			offset, pdic_data->clk_offset, pdic_data->clk_offset2);
}

void s2mf301_usbpd_set_pcp_clk(void *_data, int mode)
{
    struct usbpd_data *pd_data = _data;
    struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
    struct i2c_client *i2c = pdic_data->i2c;
    u8 reg;

	s2mf301_info("%s, mode = %s(%d)\n",
		__func__, (mode ? "USB FS" : "Default"), mode);

    s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, &reg);
	if (mode)
		reg |= S2MF301_REG_PCP_CLK_SEL;
	else
		reg &= ~S2MF301_REG_PCP_CLK_SEL;
    s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, reg);
    s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, &reg);
	s2mf301_info("%s, 0x0A(0x%02x)\n", __func__, reg);
}

static void s2mf301_usbpd_init_tx_hard_reset(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 intr[S2MF301_MAX_NUM_INT_STATUS] = {0};
	u8 reg_data;

	s2mf301_info("%s, ++\n", __func__);

	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_RETRANS, &reg_data);
	reg_data &= ~S2MF301_REG_RETRANS_MASK;
	reg_data |= RETRANS_0 << S2MF301_REG_RETRANS_SHIFT;
	s2mf301_usbpd_write_reg(pdic_data->i2c, S2MF301_REG_RETRANS, reg_data);

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_MSG_SEND_CON, S2MF301_REG_MSG_SEND_CON_SOP_HardRST
			| S2MF301_REG_MSG_SEND_CON_OP_MODE);

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_MSG_SEND_CON, S2MF301_REG_MSG_SEND_CON_SOP_HardRST
			| S2MF301_REG_MSG_SEND_CON_OP_MODE
			| S2MF301_REG_MSG_SEND_CON_SEND_MSG_EN);

	usleep_range(1000, 1100);

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_MSG_SEND_CON, S2MF301_REG_MSG_SEND_CON_OP_MODE
			| S2MF301_REG_MSG_SEND_CON_HARD_EN);

	udelay(1);
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_MSG_SEND_CON, S2MF301_REG_MSG_SEND_CON_HARD_EN);

	usleep_range(4000, 4100);

	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_RETRANS, &reg_data);
	reg_data &= ~S2MF301_REG_RETRANS_MASK;
	reg_data |= RETRANS_3 << S2MF301_REG_RETRANS_SHIFT;
	s2mf301_usbpd_write_reg(pdic_data->i2c, S2MF301_REG_RETRANS, reg_data);
	s2mf301_self_soft_reset(pdic_data);

	s2mf301_usbpd_bulk_read(i2c, S2MF301_REG_INT_STATUS0,
			S2MF301_MAX_NUM_INT_STATUS, intr);

	s2mf301_info("%s, --, clear status[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]\n",
			__func__, intr[0], intr[1], intr[2], intr[3], intr[4],
			intr[5], intr[6], intr[7], intr[8], intr[9]);

	pdic_data->status_reg[0] = pdic_data->status_reg[1] = 0;
}

void s2mf301_rprd_mode_change(void *data, u8 mode)
{
	struct s2mf301_usbpd_data *usbpd_data = data;
	struct i2c_client *i2c = usbpd_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	u8 data_reg = 0;

	s2mf301_info("%s, mode=0x%x\n", __func__, mode);

	mutex_lock(&usbpd_data->_mutex);
	if (usbpd_data->lpm_mode)
		goto skip;

	s2mf301_info("%s, start, %d\n", __func__, __LINE__);
	switch (mode) {
	case TYPE_C_ATTACH_DFP: /* SRC */
		s2mf301_usbpd_set_pd_control(usbpd_data, USBPD_CC_OFF);
		s2mf301_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP0);
		s2mf301_assert_rp(pd_data);
		msleep(20);
		s2mf301_usbpd_detach_init(usbpd_data);
		s2mf301_usbpd_notify_detach(usbpd_data);
		msleep(600);
		s2mf301_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP80);
		msleep(S2MF301_ROLE_SWAP_TIME_MS);
		s2mf301_assert_drp(pd_data);
		SET_STATUS(usbpd_data->status_reg, PLUG_ATTACH);
		schedule_delayed_work(&usbpd_data->plug_work, 0);
		break;
	case TYPE_C_ATTACH_UFP: /* SNK */
		s2mf301_usbpd_set_pd_control(usbpd_data, USBPD_CC_OFF);
		s2mf301_assert_rp(pd_data);
		s2mf301_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP0);
		msleep(20);
		s2mf301_usbpd_detach_init(usbpd_data);
		s2mf301_usbpd_notify_detach(usbpd_data);
		msleep(600);
		s2mf301_assert_rd(pd_data);
		s2mf301_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP80);
		msleep(S2MF301_ROLE_SWAP_TIME_MS);
		s2mf301_assert_drp(pd_data);
		SET_STATUS(usbpd_data->status_reg, PLUG_ATTACH);
		schedule_delayed_work(&usbpd_data->plug_work, 0);
		break;
	case TYPE_C_ATTACH_DRP: /* DRP */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data_reg);
		data_reg |= S2MF301_REG_PLUG_CTRL_DRP;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data_reg);
		break;
	};
skip:
	s2mf301_info("%s, end\n", __func__);
	mutex_unlock(&usbpd_data->_mutex);
}
EXPORT_SYMBOL(s2mf301_rprd_mode_change);



#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_PM_S2MF301)
static void s2mf301_usbpd_set_pmeter_mode(struct s2mf301_usbpd_data *pdic_data,
																int mode)
{
	struct power_supply *psy_pm = pdic_data->psy_pm;
	union power_supply_propval val;
	int ret = 0;

	s2mf301_info("%s, mode=%d\n", __func__, mode);

	if (psy_pm) {
		val.intval = mode;
		ret = psy_pm->desc->set_property(psy_pm,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_CO_ENABLE, &val);
	} else {
		s2mf301_err("%s: Fail to get pmeter\n", __func__);
		return;
	}

	if (ret) {
		s2mf301_err("%s: Fail to set pmeter\n", __func__);
		return;
	}
}

static int s2mf301_usbpd_get_pmeter_volt(struct s2mf301_usbpd_data *pdic_data)
{
	struct power_supply *psy_pm = pdic_data->psy_pm;
	union power_supply_propval val;
	int ret = 0;

	if (!psy_pm) {
		s2mf301_info("%s, psy_pm is null, try get_psy_pm\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		psy_pm = get_power_supply_by_name("s2mf301-pmeter");
#endif
		if (!psy_pm) {
			s2mf301_err("%s: Fail to get psy_pm\n", __func__);
			return -1;
		}
		pdic_data->psy_pm = psy_pm;
	}

	ret = psy_pm->desc->get_property(psy_pm,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_VCHGIN, &val);

	if (ret) {
		s2mf301_err("%s: fail to get psy_pm prop, ret(%d)\n", __func__, ret);
		return -1;
	}

	pdic_data->pm_chgin = val.intval;

	return 0;
}

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
static int s2mf301_usbpd_get_gpadc_volt(struct s2mf301_usbpd_data *pdic_data)
{
	struct power_supply *psy_pm = pdic_data->psy_pm;
	struct power_supply *psy_muic = pdic_data->psy_muic;
	union power_supply_propval val;
	int ret = 0;

	if (psy_pm) {
		ret = psy_pm->desc->get_property(psy_pm,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_VGPADC, &val);
	} else {
		s2mf301_err("%s: Fail to get pmeter\n", __func__);
		return -1;
	}

	if (ret) {
		s2mf301_err("%s: fail to set power_suppy pmeter property(%d)\n",
				__func__, ret);
		return -1;
	}

	pdic_data->pm_vgpadc = val.intval;

	s2mf301_info("%s, vGPADC = %dmV\n", __func__, pdic_data->pm_vgpadc);

	if (!psy_muic) {
		psy_muic = pdic_data->psy_muic = get_power_supply_by_name("muic-manager");
	}

	if (psy_muic) {
		ret = psy_muic->desc->get_property(psy_muic,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_VGPADC, &val);
	} else {
		s2mf301_err("%s: Fail to get muic-manager\n", __func__);
		return -1;
	}

	if (ret) {
		s2mf301_err("%s: fail to set power_suppy muic-manager property(%d)\n",
				__func__, ret);
		return -1;
	}

	pr_info("%s, SBU OVP State %d\n", __func__, val.intval);

	if (val.intval)
		pdic_data->pm_vgpadc = 5000;

	return 0;
}
#endif

static int s2mf301_usbpd_check_vbus(struct s2mf301_usbpd_data *pdic_data,
												int volt, PDIC_VBUS_SEL mode)
{
	int retry = 100;
	int i = 0;
	int ret = 0;

	if (mode == VBUS_OFF) {
		for (i = 0; i < retry; i++) {
			ret = s2mf301_usbpd_get_pmeter_volt(pdic_data);
			if (ret < 0)
				return ret;

			if (pdic_data->pm_chgin < volt) {
				s2mf301_info("%s chgin volt(%d) finish!\n", __func__,
												pdic_data->pm_chgin);
				return true;
			} else {
				s2mf301_info("%s chgin volt(%d) waiting 730ms!\n",
										__func__, pdic_data->pm_chgin);
				msleep(730);
				return true;
			}
		}
	} else if (mode == VBUS_ON) {
		ret = s2mf301_usbpd_get_pmeter_volt(pdic_data);
		if (ret < 0)
			return ret;
		if (pdic_data->pm_chgin > volt) {
			s2mf301_info("%s vbus volt(%d->%d) mode(%d)!\n",
					__func__, volt, pdic_data->pm_chgin, mode);
			return true;
		} else
			return false;
	}

	return false;
}
#endif

static int s2mf301_usbpd_check_accessory(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val, cc1_val, cc2_val;
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);

	cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

	if (cc1_val == USBPD_Rd && cc2_val == USBPD_Rd) {
		s2mf301_info("%s : Debug Accessory\n", __func__);
		return -1;
	}
	if (cc1_val == USBPD_Ra && cc2_val == USBPD_Ra) {
		s2mf301_info("%s : Audio Accessory\n", __func__);
		usbpd_manager_set_analog_audio(pd_data);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_USB_ANALOGAUDIO;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
		return -1;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static void process_dr_swap(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();

	send_otg_notify(o_notify, NOTIFY_EVENT_DR_SWAP, 1);
#endif

	dev_info(&i2c->dev, "%s : before - is_host : %d, is_client : %d\n",
		__func__, pdic_data->is_host, pdic_data->is_client);
	if (pdic_data->is_host == HOST_ON) {
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
				PDIC_NOTIFY_ID_ATTACH, 1/*attach*/, 0/*rprd*/, 0);
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				1/*attach*/, USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
		pdic_data->is_host = HOST_OFF;
		pdic_data->is_client = CLIENT_ON;
	} else if (pdic_data->is_client == CLIENT_ON) {
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
				PDIC_NOTIFY_ID_ATTACH, 1/*attach*/, 1/*rprd*/, 0);
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				1/*attach*/, USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/, 0);
		pdic_data->is_host = HOST_ON;
		pdic_data->is_client = CLIENT_OFF;
	}
	dev_info(&i2c->dev, "%s : after - is_host : %d, is_client : %d\n",
		__func__, pdic_data->is_host, pdic_data->is_client);
}
#endif

static void s2mf301_pr_swap(void *_data, int val)
{
	struct usbpd_data *pd_data = (struct usbpd_data *) _data;
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	if (val == USBPD_SINK_OFF) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC;
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.sink_status.current_pdo_num = 0;
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_BATT,
			PDIC_NOTIFY_ID_POWER_STATUS, 0, 0, 0);
#endif
	} else if (val == USBPD_SOURCE_ON) {
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
#elif IS_ENABLED(CONFIG_TYPEC)
		pd_data->typec_power_role = TYPEC_SOURCE;
		typec_set_pwr_role(pd_data->port, pd_data->typec_power_role);
#endif
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 1);
#endif
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
			PDIC_NOTIFY_ID_ROLE_SWAP, 1/* source */, 0, 0);
	} else if (val == USBPD_SOURCE_OFF) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SRCTOSNK;
		pd_data->pd_noti.sink_status.selected_pdo_num = 0;
		pd_data->pd_noti.sink_status.current_pdo_num = 0;
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_BATT,
			PDIC_NOTIFY_ID_POWER_STATUS, 0, 0, 0);
#endif

#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SNK;
#elif IS_ENABLED(CONFIG_TYPEC)
		pd_data->typec_power_role = TYPEC_SINK;
		typec_set_pwr_role(pd_data->port, pd_data->typec_power_role);
#endif
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
			PDIC_NOTIFY_ID_ROLE_SWAP, 0/* sink */, 0, 0);
	}
}

static int s2mf301_usbpd_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}
	ret &= 0xff;
	*dest = ret;
	return 0;
}

static int s2mf301_usbpd_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if IS_ENABLED(CONFIG_SEC_FACTORY) || IS_ENABLED(CONFIG_ARCH_QCOM)
	int retry = 0;
#endif

	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
#if IS_ENABLED(CONFIG_SEC_FACTORY) || IS_ENABLED(CONFIG_ARCH_QCOM)
	for (retry = 0; retry < 5; retry++) {
		if (ret < 0) {
			dev_err(dev, "%s reg(0x%x), ret(%d) retry(%d) after now\n",
							__func__, reg, ret, retry);
			msleep(40);
			ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		} else
			break;
	}

	if (ret < 0) {
		dev_err(dev, "%s failed to read reg, ret(%d)\n", __func__, ret);
#else
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#endif

#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}

static int s2mf301_usbpd_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}
	return ret;
}

static int s2mf301_usbpd_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_UPDATE_BIT_S2MF301)
static int s2mf301_usbpd_update_bit(struct i2c_client *i2c,
			u8 reg, u8 mask, u8 shift, u8 value)
{
	int ret;
	u8 reg_val = 0;

	ret = s2mf301_usbpd_read_reg(i2c, reg, &reg_val);
	if (ret < 0) {
		s2mf301_err("%s: Reg = 0x%X, val = 0x%X, read err : %d\n",
			__func__, reg, reg_val, ret);
	}
	reg_val &= ~mask;
	reg_val |= value << shift;
	ret = s2mf301_usbpd_write_reg(i2c, reg, reg_val);
	if (ret < 0) {
		s2mf301_err("%s: Reg = 0x%X, mask = 0x%X, val = 0x%X, write err : %d\n",
			__func__, reg, mask, value, ret);
	}

	return ret;
}
#endif

static int s2mf301_write_msg_all(struct i2c_client *i2c, int count, u8 *buf)
{
	int ret;

	ret = s2mf301_usbpd_bulk_write(i2c, S2MF301_REG_MSG_TX_HEADER_L,
												2 + (count * 4), buf);

	return ret;
}

static int s2mf301_send_msg(struct i2c_client *i2c)
{
	int ret;
	u8 reg = S2MF301_REG_MSG_SEND_CON;
	u8 val = S2MF301_REG_MSG_SEND_CON_OP_MODE
			| S2MF301_REG_MSG_SEND_CON_SEND_MSG_EN
			| S2MF301_REG_MSG_SEND_CON_HARD_EN;

	s2mf301_usbpd_write_reg(i2c, reg, val);

	ret = s2mf301_usbpd_write_reg(i2c, reg, S2MF301_REG_MSG_SEND_CON_OP_MODE
										| S2MF301_REG_MSG_SEND_CON_HARD_EN);

	return ret;
}

static int s2mf301_read_msg_header(struct i2c_client *i2c, msg_header_type *header)
{
	int ret;

	ret = s2mf301_usbpd_bulk_read(i2c, S2MF301_REG_MSG_RX_HEADER_L, 2, header->byte);

	return ret;
}

static int s2mf301_read_msg_obj(struct i2c_client *i2c, int count, data_obj_type *obj)
{
	int ret = 0;
	int i = 0;
	struct device *dev = &i2c->dev;

	if (count > S2MF301_MAX_NUM_MSG_OBJ) {
		dev_err(dev, "%s, not invalid obj count number\n", __func__);
		ret = -EINVAL; /*TODO: check fail case */
	} else {
		for (i = 0; i < count; i++) {
			ret = s2mf301_usbpd_bulk_read(i2c,
				S2MF301_REG_MSG_RX_OBJECT0_0_L + (4 * i),
							4, obj[i].byte);
		}
	}

	return ret;
}

static void s2mf301_set_irq_enable(struct s2mf301_usbpd_data *_data,
		u8 int0, u8 int1, u8 int2, u8 int3, u8 int4, u8 int5)
{
	u8 int_mask[S2MF301_MAX_NUM_INT_STATUS]
		= {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	int ret = 0;
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;

	s2mf301_info("%s, enter, en : %d\n", __func__, int0);
	if (_data->checking_pm_water) {
		s2mf301_info("%s, checking water, masking PLUG\n", __func__);
		int4 = ENABLED_INT_4_WATER;
	}

	int_mask[0] &= ~int0;
	int_mask[1] &= ~int1;
	int_mask[2] &= ~int2;
	int_mask[3] &= ~int3;
	int_mask[4] &= ~int4;
	int_mask[5] &= ~int5;

	ret = i2c_smbus_write_i2c_block_data(i2c, S2MF301_REG_INT_MASK0,
			S2MF301_MAX_NUM_INT_STATUS, int_mask);

	s2mf301_info("%s, ret(%d), {%2x, %2x, %2x, %2x, %2x, %2x}\n", __func__, ret,
			int_mask[0], int_mask[1], int_mask[2],
			int_mask[3], int_mask[4], int_mask[5]);

	if (ret < 0)
		dev_err(dev, "err write interrupt mask\n");
}

static void s2mf301_self_soft_reset(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;

	pdic_data->first_goodcrc = 0;

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ETC,
			S2MF301_REG_ETC_SOFT_RESET_EN);
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ETC,
			S2MF301_REG_ETC_SOFT_RESET_DIS);
}

static void s2mf301_driver_reset(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	int i;

	pdic_data->status_reg[0] = pdic_data->status_reg[1] = 0;
	data->wait_for_msg_arrived = 0;
	pdic_data->header.word = 0;
	for (i = 0; i < S2MF301_MAX_NUM_MSG_OBJ; i++)
		pdic_data->obj[i].object = 0;

	s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
			ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
}

static void s2mf301_assert_drp(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);
}

static void s2mf301_assert_rd(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	if (pdic_data->cc1_val == 2) {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
		val = (val & ~S2MF301_REG_PLUG_CTRL_PD_MANUAL_MASK) |
				S2MF301_REG_PLUG_CTRL_PD1_MANUAL_ON;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

		if (pdic_data->vconn_en) {
			s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
			val = (val & ~S2MF301_REG_PLUG_CTRL_PD_MANUAL_MASK) |
					S2MF301_REG_PLUG_CTRL_RpRd_PD2_VCONN |
					S2MF301_REG_PLUG_CTRL_VCONN_MANUAL_EN;
			s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);
		}
	}

	if (pdic_data->cc2_val == 2) {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
		val = (val & ~S2MF301_REG_PLUG_CTRL_PD_MANUAL_MASK) |
				S2MF301_REG_PLUG_CTRL_PD2_MANUAL_ON;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

		if (pdic_data->vconn_en) {
			s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
			val = (val & ~S2MF301_REG_PLUG_CTRL_PD_MANUAL_MASK) |
					S2MF301_REG_PLUG_CTRL_RpRd_PD1_VCONN |
					S2MF301_REG_PLUG_CTRL_VCONN_MANUAL_EN;
			s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);
		}
	}

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
	val |= S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);
}

static void s2mf301_assert_rp(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SRC;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
	val |= S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);
}

static unsigned s2mf301_get_status(void *_data, u64 flag)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	u64 one = 1;

	if (flag >= 64) {
		flag -= 64;
		if (pdic_data->status_reg[1] & (one << flag)) {
			s2mf301_info("%s, flag>=64 so flag-=64, flag = %llu\n", __func__, flag);
			s2mf301_info("%s, status_reg[0] (0x%llx), status_reg[1] (0x%llx)\n", __func__,
				pdic_data->status_reg[0], pdic_data->status_reg[1]);
			pdic_data->status_reg[1] &= ~(one << flag); /* clear the flag */
			return 1;
		} else
			return 0;
	} else {
		if (pdic_data->status_reg[0] & (one << flag)) {
			s2mf301_info("%s, flag=%llu, status_reg[0] (0x%llx), status_reg[1] (0x%llx)\n", __func__,
				flag, pdic_data->status_reg[0], pdic_data->status_reg[1]);
			pdic_data->status_reg[0] &= ~(one << flag); /* clear the flag */
			return 1;
		} else
			return 0;
	}
}

static bool s2mf301_poll_status(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[S2MF301_MAX_NUM_INT_STATUS] = {0};
	int ret = 0, retry = 0;
	u64 status_reg_val[2] = {0};
	msg_header_type header;
	int data_obj_num = 0;

	ret = s2mf301_usbpd_bulk_read(i2c, S2MF301_REG_INT_STATUS0,
			S2MF301_MAX_NUM_INT_STATUS, intr);

	dev_info(dev, "%s status[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]\n",
			__func__, intr[0], intr[1], intr[2], intr[3], intr[4], intr[5], intr[6]);

	if (intr[7] || intr[8] || intr[9])
		dev_info(dev, "%s, status[0x%x 0x%x 0x%x]\n", __func__, intr[7], intr[8], intr[9]);

	if ((intr[0] | intr[1] | intr[2] | intr[3] | intr[4] | intr[5]) == 0)
		goto out;

	/* GOODCRC with MSG_PASS, when first goodcrc of src_cap
	 * but, if with request, pass is valid */
	if ((intr[0] & S2MF301_REG_INT_STATUS0_MSG_GOODCRC) &&
		(pdic_data->first_goodcrc == 0)) {
		pdic_data->first_goodcrc = 1;
		if ((intr[4] & S2MF301_REG_INT_STATUS4_MSG_PASS) &&
			!(intr[2] & S2MF301_REG_INT_STATUS2_MSG_REQUEST)) {
			intr[4] &= ~ S2MF301_REG_INT_STATUS4_MSG_PASS;
		}
	}

	if ((intr[2] & S2MF301_REG_INT_STATUS2_WAKEUP) ||
		(intr[4] & S2MF301_REG_INT_STATUS4_PD12_DET_IRQ))
		s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	/* when occur detach & attach atomic */
	if (intr[4] & S2MF301_REG_INT_STATUS4_USB_DETACH) {
		SET_STATUS(status_reg_val, PLUG_DETACH);
	}

	mutex_lock(&pdic_data->lpm_mutex);
	if ((intr[4] & S2MF301_REG_INT_STATUS4_PLUG_IRQ) &&
			!pdic_data->lpm_mode && !pdic_data->is_water_detect) {
		SET_STATUS(status_reg_val, PLUG_ATTACH);
	} else if (pdic_data->lpm_mode &&
				(intr[4] & S2MF301_REG_INT_STATUS4_PLUG_IRQ) &&
									!pdic_data->is_water_detect) {
		retry = 1;
	}
	mutex_unlock(&pdic_data->lpm_mutex);

	if (retry) {
		msleep(40);
		mutex_lock(&pdic_data->lpm_mutex);
		if ((intr[4] & S2MF301_REG_INT_STATUS4_PLUG_IRQ) &&
				!pdic_data->lpm_mode && !pdic_data->is_water_detect)
			SET_STATUS(status_reg_val, PLUG_ATTACH);
		mutex_unlock(&pdic_data->lpm_mutex);
	}

	if (intr[5] & S2MF301_REG_INT_STATUS5_HARD_RESET)
		SET_STATUS(status_reg_val, MSG_HARDRESET);

	if (intr[0] & S2MF301_REG_INT_STATUS0_MSG_GOODCRC)
		SET_STATUS(status_reg_val, MSG_GOODCRC);

	if (intr[1] & S2MF301_REG_INT_STATUS1_MSG_PR_SWAP)
		SET_STATUS(status_reg_val, MSG_PR_SWAP);

	if (intr[2] & S2MF301_REG_INT_STATUS2_MSG_SOFTRESET)
		s2mf301_info("%s, Rx SOFTRESET\n", __func__);

	if (intr[1] & S2MF301_REG_INT_STATUS1_MSG_DR_SWAP)
		SET_STATUS(status_reg_val, MSG_DR_SWAP);

	if (intr[0] & S2MF301_REG_INT_STATUS0_MSG_ACCEPT) {
		SET_STATUS(status_reg_val, MSG_ACCEPT);
		if (pdic_data->check_pps_irq)
			pdic_data->check_pps_irq_msg++;
	}

	if (intr[1] & S2MF301_REG_INT_STATUS1_MSG_PSRDY) {
		SET_STATUS(status_reg_val, MSG_PSRDY);
		if (pdic_data->check_pps_irq)
			pdic_data->check_pps_irq_msg++;
	}

#if 0
	if (intr[2] & S2MF301_REG_INT_STATUS2_MSG_REQUEST)
		status_reg_val |= one << MSG_REQUEST;
#endif

	if (intr[1] & S2MF301_REG_INT_STATUS1_MSG_REJECT)
		SET_STATUS(status_reg_val, MSG_REJECT);

	if (intr[2] & S2MF301_REG_INT_STATUS2_MSG_WAIT)
		SET_STATUS(status_reg_val, MSG_WAIT);

	if (intr[4] & S2MF301_REG_INT_STATUS4_MSG_ERROR)
		SET_STATUS(status_reg_val, MSG_ERROR);

	if (intr[1] & S2MF301_REG_INT_STATUS1_MSG_PING)
		SET_STATUS(status_reg_val, MSG_PING);

	if (intr[1] & S2MF301_REG_INT_STATUS1_MSG_GETSNKCAP)
		SET_STATUS(status_reg_val, MSG_GET_SNK_CAP);

	if (intr[1] & S2MF301_REG_INT_STATUS1_MSG_GETSRCCAP)
		SET_STATUS(status_reg_val, MSG_GET_SRC_CAP);

	if (intr[2] & S2MF301_REG_INT_STATUS2_MSG_VCONN_SWAP)
		SET_STATUS(status_reg_val, MSG_VCONN_SWAP);

	if (intr[3] & S2MF301_REG_INT_STATUS3_UNS_CMD_DATA) {
		if (pdic_data->detach_valid)
			SET_STATUS(status_reg_val, PLUG_ATTACH);
		SET_STATUS(status_reg_val, MSG_RID);
	}

	/* function that support dp control */
	if (intr[4] & S2MF301_REG_INT_STATUS4_MSG_PASS) {
		if ((intr[3] & S2MF301_REG_INT_STATUS3_UNS_CMD_DATA) == 0) {
			if (usbpd_receive_msg(data)) {
				header = data->protocol_rx.msg_header;
				data_obj_num = header.num_data_objs;

				s2mf301_usbpd_check_msg(data, status_reg_val);
			}
		}
	}
out:
	s2mf301_info("%s, reg[0] before(0x%llx), new (0x%llx)\n",
		__func__, pdic_data->status_reg[0], status_reg_val[0]);
	s2mf301_info("%s, reg[1] before(0x%llx), new (0x%llx)\n",
		__func__, pdic_data->status_reg[1], status_reg_val[1]);
	pdic_data->status_reg[0] |= status_reg_val[0];
	pdic_data->status_reg[1] |= status_reg_val[1];

	return 0;
}

static void s2mf301_soft_reset(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	s2mf301_self_soft_reset(pdic_data);
}

static int s2mf301_hard_reset(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret;
	u8 reg, reg_data;

	s2mf301_info("%s,\n", __func__);

	if (pdic_data->rid != REG_RID_UNDF && pdic_data->rid != REG_RID_MAX)
		return 0;

	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_RETRANS, &reg_data);
	reg_data &= ~S2MF301_REG_RETRANS_MASK;
	reg_data |= RETRANS_0 << S2MF301_REG_RETRANS_SHIFT;
	s2mf301_usbpd_write_reg(pdic_data->i2c, S2MF301_REG_RETRANS, reg_data);

	reg = S2MF301_REG_MSG_SEND_CON;

	ret = s2mf301_usbpd_write_reg(i2c, reg, S2MF301_REG_MSG_SEND_CON_SOP_HardRST
			| S2MF301_REG_MSG_SEND_CON_OP_MODE);
	if (ret < 0)
		goto fail;

	ret = s2mf301_usbpd_write_reg(i2c, reg, S2MF301_REG_MSG_SEND_CON_SOP_HardRST
			| S2MF301_REG_MSG_SEND_CON_OP_MODE
			| S2MF301_REG_MSG_SEND_CON_SEND_MSG_EN);
	if (ret < 0)
		goto fail;

	usleep_range(200, 250);

	ret = s2mf301_usbpd_write_reg(i2c, reg, S2MF301_REG_MSG_SEND_CON_OP_MODE
										| S2MF301_REG_MSG_SEND_CON_HARD_EN);
	udelay(1);
	ret = s2mf301_usbpd_write_reg(i2c, reg, S2MF301_REG_MSG_SEND_CON_HARD_EN);
	if (ret < 0)
		goto fail;

	usleep_range(3000, 3100);


	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_RETRANS, &reg_data);
	reg_data &= ~S2MF301_REG_RETRANS_MASK;
	reg_data |= RETRANS_3 << S2MF301_REG_RETRANS_SHIFT;
	s2mf301_usbpd_write_reg(pdic_data->i2c, S2MF301_REG_RETRANS, reg_data);
	s2mf301_self_soft_reset(pdic_data);

	pdic_data->status_reg[0] = pdic_data->status_reg[1] = 0;

	return 0;

fail:
	return -EIO;
}

static int s2mf301_receive_message(void *data)
{
	struct s2mf301_usbpd_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	int obj_num = 0;
	int ret = 0;

	ret = s2mf301_read_msg_header(i2c, &pdic_data->header);
	if (ret < 0)
		dev_err(dev, "%s read msg header error\n", __func__);

	if (pdic_data->header.spec_revision < 2 &&
			pdic_data->header.num_data_objs > 0 &&
			pdic_data->header.msg_type == 6) {
		usleep_range(500, 900);
		ret = s2mf301_read_msg_header(i2c, &pdic_data->header);
		if (ret < 0)
			dev_err(dev, "%s read msg header error\n", __func__);
	}

	obj_num = pdic_data->header.num_data_objs;

	if (obj_num > 0) {
		ret = s2mf301_read_msg_obj(i2c,
			obj_num, &pdic_data->obj[0]);
	}

	return ret;
}

static int s2mf301_tx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret = 0;
	int i = 0;
	int count = 0;
	u8 send_msg[30];

	s2mf301_info("%s,\n", __func__);

	/* if there is no attach, skip tx msg */
	if (pdic_data->detach_valid)
		goto done;

#if 0 /* skip to reduce time delay */
	/* using msg id counter at s2mf301 */
	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_ID_MONITOR, &reg_data);
	msg_id = reg_data & S2MF301_REG_ID_MONITOR_MSG_ID_MASK;
	header->msg_id = msg_id;
#endif

	send_msg[0] = header->byte[0];
	send_msg[1] = header->byte[1];

	count = header->num_data_objs;

	for (i = 0; i < count; i++) {
		send_msg[2 + (i * 4)] = obj[i].byte[0];
		send_msg[3 + (i * 4)] = obj[i].byte[1];
		send_msg[4 + (i * 4)] = obj[i].byte[2];
		send_msg[5 + (i * 4)] = obj[i].byte[3];
	}

	ret = s2mf301_write_msg_all(i2c, count, send_msg);
	if (ret < 0)
		goto done;

	s2mf301_send_msg(i2c);
	s2mf301_info("%s, [Tx] [0x%x] [0x%x]\n", __func__, header->word, obj[0].object);

done:
	return ret;
}

static int s2mf301_rx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	int i;
	int count = 0;

	if (!s2mf301_receive_message(pdic_data)) {
		header->word = pdic_data->header.word;
		count = pdic_data->header.num_data_objs;
		if (count > 0) {
			for (i = 0; i < count; i++)
				obj[i].object = pdic_data->obj[i].object;
		}
		pdic_data->header.word = 0; /* To clear for duplicated call */
		return 0;
	} else {
		return -EINVAL;
	}
}

static int s2mf301_set_otg_control(void *_data, int val)
{
	struct usbpd_data *pd_data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	mutex_lock(&pdic_data->pd_mutex);
	if (val) {
		if (pdic_data->is_killer == 0)
			usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_ON);
	} else
		usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_OFF);
	mutex_unlock(&pdic_data->pd_mutex);

	return 0;
}

static int s2mf301_set_chg_lv_mode(void *_data, int voltage)
{
	struct power_supply *psy_charger = NULL;
	union power_supply_propval val;
	int ret = 0;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_charger = get_power_supply_by_name("s2mf301-charger");
#endif
	if (psy_charger == NULL) {
		s2mf301_err("%s: Fail to get psy charger\n", __func__);
		return -1;
	}

	if (voltage == 5) {
		val.intval = 0;
	} else if (voltage == 9) {
		val.intval = 1;
	} else {
		s2mf301_err("%s: invalid pram:%d\n", __func__, voltage);
		return -1;
	}

	ret = psy_charger->desc->set_property(psy_charger,
		(enum power_supply_property)POWER_SUPPLY_LSI_PROP_2LV_3LV_CHG_MODE, &val);

	if (ret)
		s2mf301_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);

	return ret;
}
static int s2mf301_set_pd_control(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	int ret = 0;

	mutex_lock(&pdic_data->pd_mutex);
	ret = s2mf301_usbpd_set_pd_control(pdic_data, val);
	mutex_unlock(&pdic_data->pd_mutex);

	return ret;
}

#if IS_ENABLED(CONFIG_TYPEC)
static void s2mf301_set_pwr_opmode(void *_data, int mode)
{
	struct usbpd_data *pd_data = (struct usbpd_data *) _data;

	typec_set_pwr_opmode(pd_data->port, mode);
}
#endif

static void s2mf301_usbpd_set_is_otg_vboost(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	pdic_data->is_otg_vboost = val;

	return;
}

static void s2mf301_usbpd_irq_control(void *_data, int mode)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	if (mode)
		enable_irq(pdic_data->irq);
	else
		disable_irq(pdic_data->irq);
}

static int s2mf301_usbpd_get_detach_valid(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	return pdic_data->detach_valid;
}

static int s2mf301_usbpd_ops_get_lpm_mode(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	return pdic_data->lpm_mode;
}

static int s2mf301_usbpd_ops_get_rid(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	return pdic_data->rid;
}

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
static int s2mf301_usbpd_ops_get_is_water_detect(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	return pdic_data->is_water_detect;
}

static int s2mf301_usbpd_ops_power_off_water(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	return s2mf301_power_off_water_check(pdic_data);
}
static int s2mf301_usbpd_ops_prt_water_threshold(void *_data, char *buf)
{
	int ret;

	ret = sprintf(buf, "not defined\n");

	return ret;
}

static void s2mf301_usbpd_ops_set_water_threshold(void *_data, int val1, int val2)
{
	s2mf301_info("%s, #%d is set to %d\n", __func__, val1, val2);

	return;
}
#endif

static void s2mf301_usbpd_ops_control_option_command(void *_data, int cmd)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	s2mf301_control_option_command(pdic_data, cmd);
}

static void s2mf301_usbpd_ops_sysfs_lpm_mode(void *_data, int cmd)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	mutex_lock(&pdic_data->lpm_mutex);
#ifdef CONFIG_SEC_FACTORY
	if (cmd != 1 && cmd != 2)
		s2mf301_set_normal_mode(pdic_data);
#else
	if (cmd == 1 || cmd == 2)
		s2mf301_set_lpm_mode(pdic_data);
	else
		s2mf301_set_normal_mode(pdic_data);
#endif
	mutex_unlock(&pdic_data->lpm_mutex);

	return;
}

static void s2mf301_usbpd_energy_now(void *_data, int val)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data_1b, data_2e;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL, &data_2e);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, &data_1b);

	switch (val) {
		case S2MF301_FACTORY_1BON_2EON:
			data_1b |= S2MF301_REG_RD_OR_VBUS_MUX_SEL;
			data_2e |= S2MF301_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;
			break;
		case S2MF301_FACTORY_1BON_2EOFF:
			data_1b |= S2MF301_REG_RD_OR_VBUS_MUX_SEL;
			data_2e &= ~S2MF301_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;
			break;
		case S2MF301_FACTORY_1BOFF_2EON:
			data_1b &= ~S2MF301_REG_RD_OR_VBUS_MUX_SEL;
			data_2e |= S2MF301_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;
			break;
		case S2MF301_FACTORY_1BOFF_2EOFF:
			data_1b &= ~S2MF301_REG_RD_OR_VBUS_MUX_SEL;
			data_2e &= ~S2MF301_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;
			break;
		default:
			s2mf301_info("%s, invalid val(%d)\n", __func__, val);
			break;
	}
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL, data_2e);
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, data_1b);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL, &data_2e);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, &data_1b);
	s2mf301_info("%s, 2e(0x%x), 1b(0x%x)\n", __func__, data_2e, data_1b);
}

static void s2mf301_usbpd_authentic(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, &data);
	data &= ~(S2MF301_REG_RD_OR_VBUS_MUX_SEL);
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, data);
}

static void s2mf301_usbpd_self_reset(struct s2mf301_usbpd_data *pdic_data)
{
	u8 reg_data;

	s2mf301_usbpd_test_read(pdic_data);
	s2mf301_usbpd_read_reg(pdic_data->i2c, 0x01, &reg_data);
	reg_data |= (1<<1);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x01, reg_data);
	s2mf301_usbpd_read_reg(pdic_data->i2c, 0x01, &reg_data);
	reg_data &= ~(1<<1);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x01, reg_data);
	s2mf301_usbpd_read_reg(pdic_data->i2c, 0x01, &reg_data);

	s2mf301_usbpd_read_reg(pdic_data->i2c, 0x08, &reg_data);
	reg_data &= ~(1<<5);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x08, reg_data);
	reg_data |= (1<<5);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x08, reg_data);
	reg_data &= ~(1<<5);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x08, reg_data);

	s2mf301_usbpd_read_reg(pdic_data->i2c, 0x00, &reg_data);
	reg_data &= ~(1<<1);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x00, reg_data);
	reg_data |= (1<<1);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x00, reg_data);
	reg_data &= ~(1<<1);
	s2mf301_usbpd_write_reg(pdic_data->i2c, 0x00, reg_data);

}

static void s2mf301_usbpd_vbus_onoff(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;

	s2mf301_usbpd_self_reset(pdic_data);
}

static void s2mf301_usbpd_set_usbpd_reset(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	u8 intr[S2MF301_MAX_NUM_INT_STATUS] = {0};

	s2mf301_usbpd_self_reset(pdic_data);

	msleep(20);

	s2mf301_usbpd_test_read(pdic_data);
	s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
			ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	s2mf301_usbpd_bulk_read(pdic_data->i2c, S2MF301_REG_INT_STATUS0, S2MF301_MAX_NUM_INT_STATUS, intr);
	s2mf301_info("%s, --, clear status[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]\n",
			__func__, intr[0], intr[1], intr[2], intr[3], intr[4],
			intr[5], intr[6], intr[7], intr[8], intr[9]);

	s2mf301_set_normal_mode(pdic_data);
	s2mf301_usbpd_reg_init(pdic_data);
	s2mf301_usbpd_test_read(pdic_data);

	SET_STATUS(pdic_data->status_reg, PLUG_ATTACH);
	schedule_delayed_work(&pdic_data->plug_work, 0);
}

#if IS_ENABLED(CONFIG_S2M_PDIC_MANUAL_RETRY)
static void s2mf301_ops_set_manual_retry(void *_data, int val)
{
	struct usbpd_data *pd_data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	u8 reg_data;

	pd_data->is_manual_retry = val;
	s2mf301_info("%s, %s\n", __func__, val ? "Manual" : "Auto");

	if (val) {
		s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_RETRANS, &reg_data);
		reg_data &= ~S2MF301_REG_RETRANS_MASK;
		reg_data |= RETRANS_0 << S2MF301_REG_RETRANS_SHIFT;
		s2mf301_usbpd_write_reg(pdic_data->i2c, S2MF301_REG_RETRANS, reg_data);
	} else {
		s2mf301_set_revision(pd_data, pd_data->specification_revision);
	}
}
#endif

static int s2mf301_set_rp_control(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	mutex_lock(&pdic_data->pd_mutex);
	s2mf301_usbpd_set_rp_scr_sel(pdic_data, val);
	mutex_unlock(&pdic_data->pd_mutex);

	return 0;
}

static void s2mf301_get_rp_level(void *_data, int *ret_val)
{
	struct usbpd_data *pd_data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 cc1_val, cc2_val, val;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);

	cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

	s2mf301_info("%s, attach: cc1(%x), cc2(%x)\n", __func__, cc1_val, cc2_val);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_FSM_MON, &val);

	cc1_val = (val & S2MF301_REG_CTRL_FSM_MON_CC1_MASK) >> S2MF301_REG_CTRL_FSM_MON_CC1_SHIFT;
	cc2_val = (val & S2MF301_REG_CTRL_FSM_MON_CC2_MASK) >> S2MF301_REG_CTRL_FSM_MON_CC2_SHIFT;

	s2mf301_info("%s, rp : cc1(%x), cc2(%x)\n", __func__, cc1_val, cc2_val);

	if (cc1_val == USBPD_10k || cc2_val == USBPD_10k)
		*ret_val = RP_CURRENT_LEVEL3;
	else if (cc1_val == USBPD_22k || cc2_val == USBPD_22k)
		*ret_val = RP_CURRENT_LEVEL2;
	else if (cc1_val == USBPD_56k || cc2_val == USBPD_56k)
		*ret_val = RP_CURRENT_LEVEL_DEFAULT;
}

static int  s2mf301_pd_instead_of_vbus(void *_data, int enable)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	if(pdic_data->cc_instead_of_vbus == enable)
		return 0;

	s2mf301_info("%s, en(%d)\n", __func__, enable);

	pdic_data->cc_instead_of_vbus = enable;

	//Setting for PD Detection with VBUS
	//It is recognized that VBUS falls when PD line falls.
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, &val);
	val &= ~S2MF301_REG_RD_OR_VBUS_MUX_SEL;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, val);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL, &val);
	if (enable)
		val |= S2MF301_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;
	else
		val &= ~S2MF301_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL, val);

	return 0;
}

static int  s2mf301_op_mode_clear(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;

	u8 reg = S2MF301_REG_MSG_SEND_CON;
	u8 val = 0;

	val &= ~S2MF301_REG_MSG_SEND_CON_OP_MODE;

	s2mf301_usbpd_write_reg(i2c, reg, val);

	return 0;
}

static int s2mf301_vbus_on_check(void *_data)
{
#if IS_ENABLED(CONFIG_PM_S2MF301)
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	return s2mf301_usbpd_check_vbus(pdic_data, 4000, VBUS_ON);
#else
	return 0;
#endif
}

#if IS_ENABLED(CONFIG_CHECK_CTYPE_SIDE) || IS_ENABLED(CONFIG_PDIC_SYSFS)
static int s2mf301_get_side_check(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val, cc1_val, cc2_val;

	s2mf301_usbpd_test_read(pdic_data);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);

	cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

	if (cc1_val == USBPD_Rd)
		return USBPD_UP_SIDE;
	else if (cc2_val == USBPD_Rd)
		return USBPD_DOWN_SIDE;
	else
		return USBPD_UNDEFFINED_SIDE;
}
#endif
static int s2mf301_set_vconn_source(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0, reg_val = 0, cc1_val = 0, cc2_val = 0;

	if (!pdic_data->vconn_en) {
		s2mf301_err("%s, not support vconn source\n", __func__);
		return -1;
	}

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &reg_val);
	cc1_val = (reg_val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	cc2_val = (reg_val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

	if (val == USBPD_VCONN_ON) {
		if (cc1_val == USBPD_Rd) {
			if (cc2_val == USBPD_Ra) {
				s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &reg_data);
				reg_data &= ~S2MF301_REG_PLUG_CTRL_RpRd_VCONN_MASK;
				reg_data |= (S2MF301_REG_PLUG_CTRL_RpRd_PD2_VCONN |
						S2MF301_REG_PLUG_CTRL_VCONN_MANUAL_EN);
				s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, reg_data);
			}
		}
		if (cc2_val == USBPD_Rd) {
			if (cc1_val == USBPD_Ra) {
				s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &reg_data);
				reg_data &= ~S2MF301_REG_PLUG_CTRL_RpRd_VCONN_MASK;
				reg_data |= (S2MF301_REG_PLUG_CTRL_RpRd_PD1_VCONN |
						S2MF301_REG_PLUG_CTRL_VCONN_MANUAL_EN);
				s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, reg_data);
			}
		}
	} else if (val == USBPD_VCONN_OFF) {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &reg_data);
				reg_data &= ~S2MF301_REG_PLUG_CTRL_RpRd_VCONN_MASK;
		reg_data |= S2MF301_REG_PLUG_CTRL_VCONN_MANUAL_EN;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, reg_data);
	} else
		return(-1);

	pdic_data->vconn_source = val;
	return 0;
}

static void s2mf301_usbpd_set_check_facwater(void *_data, int val)
{
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	struct usbpd_data *pd_data = (struct usbpd_data *)_data;
	struct s2mf301_usbpd_data *pdic_data = (struct s2mf301_usbpd_data *)pd_data->phy_driver_data;

	if (val) {
		pdic_data->facwater_check_cnt = 0;
		cancel_delayed_work(&pdic_data->check_facwater);
		schedule_delayed_work(&pdic_data->check_facwater, msecs_to_jiffies(500));
	} else {
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MANAGER,
				PDIC_NOTIFY_ID_WATER, 0, 0, 0);
	}
#endif
	return;
}

static void s2mf301_usbpd_set_vconn_manual(struct s2mf301_usbpd_data *pdic_data, bool enable)
{
	u8 reg_data = 0;

	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_PLUG_CTRL_RpRd, &reg_data);
	reg_data &= ~S2MF301_REG_PLUG_CTRL_RpRd_VCONN_MASK;

	if (enable)
		reg_data |= S2MF301_REG_PLUG_CTRL_VCONN_MANUAL_EN;

	s2mf301_usbpd_write_reg(pdic_data->i2c, S2MF301_REG_PLUG_CTRL_RpRd, reg_data);
}

static int s2mf301_get_vconn_source(void *_data, int *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	/* TODO
		set s2mf301 pdic register control */

	if (pdic_data->vconn_source != *val) {
		dev_info(pdic_data->dev, "%s, vconn_source(%d) != gpio val(%d)\n",
				__func__, pdic_data->vconn_source, *val);
		pdic_data->vconn_source = *val;
	}

	return 0;
}

static void s2mf301_set_revision(void *_data, int val)
{
	struct usbpd_data *pd_data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	u8 reg_data = 0;

#if IS_ENABLED(CONFIG_S2M_PDIC_MANUAL_RETRY)
	if (pd_data->is_manual_retry)
		return;
#endif

	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_RETRANS, &reg_data);
	reg_data &= ~S2MF301_REG_RETRANS_MASK;
	if (val == USBPD_PD3_0) {
		reg_data |= RETRANS_2 << S2MF301_REG_RETRANS_SHIFT;
		pd_data->specification_revision = USBPD_PD3_0;
	} else if (val == USBPD_PD2_0) {
		reg_data |= RETRANS_3 << S2MF301_REG_RETRANS_SHIFT;
		pd_data->specification_revision = USBPD_PD2_0;
	}
	s2mf301_usbpd_write_reg(pdic_data->i2c, S2MF301_REG_RETRANS, reg_data);

}

/* val : sink(0) or source(1) */
static int s2mf301_set_power_role(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	s2mf301_info("%s, power_role(%d)\n", __func__, val);

	if (val == USBPD_SINK) {
		pdic_data->power_role = val;
		pdic_data->is_pr_swap = true;
		s2mf301_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP_MAX);
		s2mf301_assert_rd(data);
		s2mf301_snk(pdic_data->i2c);
	} else if (val == USBPD_SOURCE) {
		pdic_data->power_role = val;
		pdic_data->is_pr_swap = true;
		s2mf301_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP_MAX);
		s2mf301_assert_rp(data);
		s2mf301_src(pdic_data->i2c);
	} else if (val == USBPD_DRP) {
		pdic_data->is_pr_swap = false;
		s2mf301_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP_MAX);
		usleep_range(2000, 2100);
		s2mf301_assert_drp(data);
		return 0;
	} else
		return(-1);

	return 0;
}

static int s2mf301_get_power_role(void *_data, int *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->power_role;
	return 0;
}

static int s2mf301_set_data_role(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val_port, data_role;

	/* DATA_ROLE (0x18[2])
	 * 0 : UFP
	 * 1 : DFP
	 */
	if (val == USBPD_UFP) {
		data_role = S2MF301_REG_MSG_DATA_ROLE_UFP;
		s2mf301_ufp(i2c);
	} else {/* (val == USBPD_DFP) */
		data_role = S2MF301_REG_MSG_DATA_ROLE_DFP;
		s2mf301_dfp(i2c);
	}

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, &val_port);
	val_port = (val_port & ~S2MF301_REG_MSG_DATA_ROLE_MASK) | data_role;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, val_port);

	pdic_data->data_role = val;

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	process_dr_swap(pdic_data);
#endif
	return 0;
}

static int s2mf301_get_data_role(void *_data, int *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->data_role;
	return 0;
}

static void s2mf301_get_vbus_short_check(void *_data, bool *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->vbus_short;
}

static void s2mf301_pd_vbus_short_check(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;

	if (pdic_data->pd_vbus_short_check)
		return;

	pdic_data->vbus_short_check = false;

	s2mf301_vbus_short_check(pdic_data);

	pdic_data->pd_vbus_short_check = true;
}

#if IS_ENABLED(CONFIG_HICCUP_CC_DISABLE)
static void s2mf301_usbpd_ops_ccopen_req(void *_data, int val)
{
	struct usbpd_data *pd_data = (struct usbpd_data *)_data;
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
	int before = pdic_data->is_manual_cc_open;

	if (val)
		pdic_data->is_manual_cc_open |= 1 << CC_OPEN_OVERHEAT;
	else
		pdic_data->is_manual_cc_open &= ~(1 << CC_OPEN_OVERHEAT);

	s2mf301_info("%s, ccopen 0x%x -> 0x%x\n", __func__, before,
			pdic_data->is_manual_cc_open);

	if (val) {
		/*
		 * Rp + 0uA -> always in vRa -> Audio Acc Attach
		 * turn off SupportACC -> change otpmode -> IRQ not occured
		 */

		s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4_CCOPEN, ENABLED_INT_5);
		s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_OPEN);
	}
	else {
		if (pdic_data->is_water_detect) {
			s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_RD);
		} else {
			/*
			 * turn on SupportACC -> change otpmode -> can IRQ occured
			 */
			s2mf301_set_irq_enable(pdic_data,
					ENABLED_INT_0, ENABLED_INT_1,
					ENABLED_INT_2, ENABLED_INT_3,
					ENABLED_INT_4, ENABLED_INT_5);
			s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_DRP);
		}
	}
}
#endif

static void s2mf301_usbpd_set_threshold(struct s2mf301_usbpd_data *pdic_data,
			PDIC_RP_RD_SEL port_sel, PDIC_THRESHOLD_SEL threshold_sel)
{
	struct i2c_client *i2c = pdic_data->i2c;

	if (threshold_sel > S2MF301_THRESHOLD_MAX) {
		dev_err(pdic_data->dev, "%s : threshold overflow!!\n", __func__);
		return;
	} else {
		pdic_data->slice_lvl[port_sel] = threshold_sel;
		s2mf301_info("%s, %s : %dmV\n", __func__, ((port_sel == PLUG_CTRL_RD)?"RD":"RP"),
				slice_mv[threshold_sel]);
		if (port_sel == PLUG_CTRL_RD)
			s2mf301_usbpd_write_reg(i2c,
				S2MF301_REG_PLUG_CTRL_SET_RD, threshold_sel | 0x40);
		else if (port_sel == PLUG_CTRL_RP)
			s2mf301_usbpd_write_reg(i2c,
				S2MF301_REG_PLUG_CTRL_SET_RP, threshold_sel);
	}
}

static int s2mf301_usbpd_check_abnormal_attach(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP,
										S2MF301_THRESHOLD_1628MV);
	usleep_range(20000, 20100);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON2, &data);
	if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SOURCE)
		return true;
	else
		return false;
}

static void s2mf301_usbpd_set_rp_scr_sel(struct s2mf301_usbpd_data *pdic_data,
							PDIC_RP_SCR_SEL scr_sel)
{
	struct i2c_client *i2c = pdic_data->i2c;

	u8 data = 0;
	s2mf301_info("%s: prev_sel(%d), scr_sel : (%d)\n", __func__, pdic_data->rp_lvl, scr_sel);

	if (pdic_data->detach_valid
			|| pdic_data->checking_pm_water) {
		dev_info(pdic_data->dev, "%s, ignore rp control detach(%d)\n",
				__func__, pdic_data->detach_valid);
		return;
	}
	if (pdic_data->is_manual_cc_open) {
		s2mf301_info("%s, CC_OPEN(0x%x)\n", __func__, pdic_data->is_manual_cc_open);
		return;
	}

	if (scr_sel == PLUG_CTRL_RP_MAX) {
		s2mf301_info("%s, refresh(%d) rp current, threshold\n", __func__, pdic_data->rp_lvl);
		scr_sel = pdic_data->rp_lvl;
	} else {
		if (pdic_data->rp_lvl == scr_sel)
			return;
		pdic_data->rp_lvl = scr_sel;
	}

	switch (scr_sel) {
	case PLUG_CTRL_RP0:
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MF301_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MF301_REG_PLUG_CTRL_RP0;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
		s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RD,
						S2MF301_THRESHOLD_214MV);
		break;
	case PLUG_CTRL_RP80:
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MF301_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MF301_REG_PLUG_CTRL_RP80;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
		s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RD,
						S2MF301_THRESHOLD_214MV);
		break;
	case PLUG_CTRL_RP180:
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MF301_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MF301_REG_PLUG_CTRL_RP180;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
		s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RD,
						S2MF301_THRESHOLD_428MV);
		break;
	case PLUG_CTRL_RP330:
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MF301_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MF301_REG_PLUG_CTRL_RP330;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
		s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RD,
						S2MF301_THRESHOLD_814MV);
		break;
	default:
		break;
	}

	if (pdic_data->power_role == USBPD_SOURCE) {
		switch (scr_sel) {
		case PLUG_CTRL_RP330:
			s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP, S2MF301_THRESHOLD_MAX);
			break;
		case PLUG_CTRL_RP0:
		case PLUG_CTRL_RP80:
		case PLUG_CTRL_RP180:
		default:
			s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP, S2MF301_THRESHOLD_1628MV);
			break;
		}
	} else if (pdic_data->power_role == USBPD_SINK || pdic_data->power_role == USBPD_DRP) {
		s2mf301_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP, S2MF301_THRESHOLD_MAX);
	} else
		s2mf301_info("%s, invalid power_role\n", __func__);

	s2mf301_info("%s, slice %dmV - %dmV\n", __func__,
			slice_mv[pdic_data->slice_lvl[0]],
			slice_mv[pdic_data->slice_lvl[1]]);

	return;
}

int s2mf301_usbpd_check_msg(void *_data, u64 *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mf301_usbpd_data *pdic_data = data->phy_driver_data;
	int data_type = 0;
	int msg_type = 0;
	int vdm_type = 0;
	int vdm_command = 0;

	dev_info(data->dev, "%s, H[0x%x]\n", __func__, data->protocol_rx.msg_header.word);

	if (data->protocol_rx.msg_header.num_data_objs == 0)
		data_type = USBPD_CTRL_MSG;
	else if (data->protocol_rx.msg_header.extended == 0)
		data_type = USBPD_DATA_MSG;
	else if (data->protocol_rx.msg_header.extended == 1)
		data_type = USBPD_EXTENDED_MSG;

	msg_type = data->protocol_rx.msg_header.msg_type;

	/* Control Message */
	if (data_type == USBPD_CTRL_MSG) {
		switch (msg_type) {
		case 0:
		case USBPD_GoodCRC:
		case USBPD_GotoMin:
			break;
		case USBPD_Accept:
			if (pdic_data->check_pps_irq)
				pdic_data->check_pps_irq_pass++;
			break;
		case USBPD_Reject:
		case USBPD_Ping:
			break;
		case USBPD_PS_RDY:
			if (pdic_data->check_pps_irq)
				pdic_data->check_pps_irq_pass++;
			break;
		case USBPD_Get_Source_Cap:
		case USBPD_Get_Sink_Cap:
			// irq status
			break;
		case USBPD_DR_Swap:
		case USBPD_PR_Swap:
		case USBPD_VCONN_Swap:
		case USBPD_Wait:
			break;
			// irq status
		case USBPD_Soft_Reset:
			SET_STATUS(val, MSG_SOFTRESET);
			break;
		case USBPD_Not_Supported:
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_NOT_SUPPORTED);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_Get_Source_Cap_Extended:
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_GET_SOURCE_CAP_EXTENDED);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_Get_Status:
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_GET_STATUS);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_FR_Swap:
			if (data->specification_revision == USBPD_PD3_0)
			/* Accept bit Clear */
				SET_STATUS(val, MSG_FR_SWAP);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_Get_PPS_Status:
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_PPS_STATUS);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_Get_Country_Codes:
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_GET_COUNTRY_CODES);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_Get_Sink_Cap_Extended:
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_GET_SINK_CAP_EXTENDED);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case 14: /* DataReset 3.1 */
			SET_STATUS(val, MSG_RESERVED);
			break;
		case 15: /* DataResetComplete 3.1 */
			SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_Get_Source_Info: /* GetSourceInfo 3.1 */
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_GET_SOURCE_INFO);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case USBPD_Get_Revision: /* GetRevision 3.1 */
			if (data->specification_revision == USBPD_PD3_0)
				SET_STATUS(val, MSG_GET_REVISION);
			else
				SET_STATUS(val, MSG_RESERVED);
			break;
		case 25 ... 31: /* Reserved */
		default:
			SET_STATUS(val, MSG_RESERVED);
			break;
		}
		dev_info(data->dev, "%s: USBPD_CTRL_MSG : %02d\n", __func__, msg_type);
	}

	/* Data Message */
	if (data_type == USBPD_DATA_MSG) {
		switch (msg_type) {
		case USBPD_Source_Capabilities:
			SET_STATUS(val, MSG_SRC_CAP);
			break;
		case USBPD_Request:
			SET_STATUS(val, MSG_REQUEST);
			break;
		case USBPD_BIST:
			SET_STATUS(val, MSG_BIST);
			break;
		case USBPD_Sink_Capabilities:
			SET_STATUS(val, MSG_SNK_CAP);
			break;
		case USBPD_Battery_Status:
			SET_STATUS(val, MSG_BATTERY_STATUS);
			break;
		case USBPD_Alert:
			SET_STATUS(val, MSG_ALERT);
			break;
		case USBPD_Get_Country_Info:
			SET_STATUS(val, MSG_GET_COUNTRY_INFO);
			break;
		case USBPD_Vendor_Defined:
			if (data->protocol_rx.data_obj[0].structured_vdm.svid != 0xFF00) {
				if (data->protocol_rx.data_obj[0].structured_vdm.svid == 0xeeee ||
						data->protocol_rx.data_obj[0].structured_vdm.svid == 0x1748 ||
							data->protocol_rx.data_obj[0].structured_vdm.svid == 0x04e8) {
					/* Ellisys VID 0xeeee */
					/* MQP VID 0x1748 */
					/* SAMSUNG VID 0x04e8 */
					dev_info(data->dev, "%s : SVID = [0x%x]\n", __func__, data->protocol_rx.data_obj[0].structured_vdm.svid);
				} else if (data->protocol_rx.data_obj[0].unstructured_vdm.vendor_id == 0x1500) {
					/* Ellisys Vendor_id 0x1500 */
					dev_info(data->dev, "%s : Vendor_id = [0x%x]\n", __func__, data->protocol_rx.data_obj[0].unstructured_vdm.vendor_id);
				} else {
					dev_info(data->dev, "%s : VDM Buffer Error\n", __func__);
					return 0;
				}
			}

			vdm_command = data->protocol_rx.data_obj[0].structured_vdm.command;
			vdm_type = data->protocol_rx.data_obj[0].structured_vdm.vdm_type;

			if (vdm_type == Unstructured_VDM) {
				if (data->protocol_rx.data_obj[0].unstructured_vdm.vendor_id != SAMSUNG_VENDOR_ID) {
					dev_info(data->dev, "%s : uvdm not samsung msg received!\n", __func__);
					SET_STATUS(val, MSG_UVDM_MSG_NOT_SAMSUNG);
					break;
				}
				dev_info(data->dev, "%s : uvdm msg received!\n", __func__);
				SET_STATUS(val, UVDM_MSG);
				break;
			}
			switch (vdm_command) {
			case DisplayPort_Status_Update:
				SET_STATUS(val, VDM_DP_STATUS_UPDATE);
				break;
			case DisplayPort_Configure:
				SET_STATUS(val, VDM_DP_CONFIGURE);
				break;
			case Attention:
				SET_STATUS(val, VDM_ATTENTION);
				break;
			case Exit_Mode:
				SET_STATUS(val, VDM_EXIT_MODE);
				break;
			case Enter_Mode:
				SET_STATUS(val, VDM_ENTER_MODE);
				break;
			case Discover_Modes:
				SET_STATUS(val, VDM_DISCOVER_MODE);
				break;
			case Discover_SVIDs:
				SET_STATUS(val, VDM_DISCOVER_SVID);
				break;
			case Discover_Identity:
				SET_STATUS(val, VDM_DISCOVER_IDENTITY);
				break;
			default:
				break;
			}
			break;
		case 0: /* Reserved */
		case 8 ... 0xe:
			SET_STATUS(val, MSG_RESERVED);
			break;
		}
		if (msg_type == USBPD_Vendor_Defined)
			dev_info(data->dev, "%s: USBPD_DATA_MSG(VDM) : %02d\n", __func__, vdm_command);
		else
			dev_info(data->dev, "%s: USBPD_DATA_MSG : %02d\n", __func__, msg_type);
	}

	/* Extended Message */
	if (data_type == USBPD_EXTENDED_MSG) {
		//MQP : PROT-SNK3-PPS
		if ((data->protocol_rx.data_obj[0].extended_msg_header_type.chunked)
			&& (data->protocol_rx.data_obj[0].extended_msg_header_type.data_size > 24)) {
			SET_STATUS(val, MSG_RESERVED);
			return 0;
		}
		switch (msg_type) {
		case USBPD_Source_Capabilities_Extended:
			SET_STATUS(val, MSG_SOURCE_CAPABILITIES_EXTENDED);
			break;
		case USBPD_Status:
			SET_STATUS(val, MSG_STATUS);
			break;
		case USBPD_Get_Battery_Cap:
			SET_STATUS(val, MSG_GET_BATTERY_CAP);
			break;
		case USBPD_Get_Battery_Status:
			SET_STATUS(val, MSG_GET_BATTERY_STATUS);
			break;
		case USBPD_Battery_Capabilities:
			SET_STATUS(val, MSG_BATTERY_CAPABILITIES);
			break;
		case USBPD_Get_Manufacturer_Info:
			SET_STATUS(val, MSG_GET_MANUFACTURER_INFO);
			break;
		case USBPD_Manufacturer_Info:
			SET_STATUS(val, MSG_MANUFACTURER_INFO);
			break;
		case USBPD_Security_Request:
			SET_STATUS(val, MSG_SECURITY_REQUEST);
			break;
		case USBPD_Security_Response:
			SET_STATUS(val, MSG_SECURITY_RESPONSE);
			break;
		case USBPD_Firmware_Update_Request:
			SET_STATUS(val, MSG_FIRMWARE_UPDATE_REQUEST);
			break;
		case USBPD_Firmware_Update_Response:
			SET_STATUS(val, MSG_FIRMWARE_UPDATE_RESPONSE);
			break;
		case USBPD_PPS_Status:
			SET_STATUS(val, MSG_PPS_STATUS);
			break;
		case USBPD_Country_Info:
			SET_STATUS(val, MSG_COUNTRY_INFO);
			break;
		case USBPD_Country_Codes:
			SET_STATUS(val, MSG_COUNTRY_CODES);
			break;
		case USBPD_Sink_Capabilities_Extended:
			SET_STATUS(val, MSG_SINK_CAPABILITIES_EXTENDED);
			break;
		default: /* Reserved */
			SET_STATUS(val, MSG_RESERVED);
			break;
		}
		dev_info(data->dev, "%s: USBPD_EXTENDED_MSG : %02d\n", __func__, msg_type);
	}

	return 0;
}

static int s2mf301_usbpd_set_pd_control(struct s2mf301_usbpd_data  *pdic_data, int val)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	dev_info(pdic_data->dev, "%s, (%d)\n", __func__, val);

	if (pdic_data->detach_valid) {
		dev_info(pdic_data->dev, "%s, ignore pd control\n", __func__);
		return 0;
	}

	if (val) {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL, &data);
		data |= S2MF301_REG_PLUG_CTRL_ECO_SRC_CAP_RDY;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL, data);
	} else {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL, &data);
		data &= ~S2MF301_REG_PLUG_CTRL_ECO_SRC_CAP_RDY;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL, data);
	}

	return 0;
}

static void s2mf301_dfp(struct i2c_client *i2c)
{
	u8 data;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, &data);
	data |= S2MF301_REG_MSG_DATA_ROLE_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, data);
}

static void s2mf301_ufp(struct i2c_client *i2c)
{
	u8 data;
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, &data);
	data &= ~S2MF301_REG_MSG_DATA_ROLE_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, data);
}

static void s2mf301_src(struct i2c_client *i2c)
{
	u8 data;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, &data);
	data = (data & ~S2MF301_REG_MSG_POWER_ROLE_MASK) | S2MF301_REG_MSG_POWER_ROLE_SOURCE;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, data);
}

static void s2mf301_snk(struct i2c_client *i2c)
{
	u8 data;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, &data);
	data = (data & ~S2MF301_REG_MSG_POWER_ROLE_MASK) | S2MF301_REG_MSG_POWER_ROLE_SINK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_MSG, data);
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
void s2mf301_control_option_command(struct s2mf301_usbpd_data *pdic_data, int cmd)
{
	struct usbpd_data *_data = dev_get_drvdata(pdic_data->dev);
	int pd_cmd = cmd & 0x0f;

/* 0x1 : Vconn control option command ON
 * 0x2 : Vconn control option command OFF
 * 0x3 : Water Detect option command ON
 * 0x4 : Water Detect option command OFF
 */
	switch (pd_cmd) {
	case 1:
		s2mf301_set_vconn_source(_data, USBPD_VCONN_ON);
		break;
	case 2:
		s2mf301_set_vconn_source(_data, USBPD_VCONN_OFF);
		break;
	case 3:
		s2mf301_usbpd_set_check_facwater(_data, 1);
		break;
	case 4:
		s2mf301_usbpd_set_check_facwater(_data, 0);
		break;
	default:
		break;
	}
}
#endif

#if IS_ENABLED(CONFIG_PDIC_MANUAL_QBAT) && !IS_ENABLED(CONFIG_SEC_FACTORY)
static void s2mf301_manual_qbat_control(struct s2mf301_usbpd_data *pdic_data, int rid)
{
	struct power_supply *psy_charger;
	union power_supply_propval val;
	int ret = 0;

	s2mf301_info("%s, rid=%d\n", __func__, rid);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_charger = get_power_supply_by_name("s2mf301-charger");
#endif

	if (psy_charger == NULL) {
		s2mf301_err("%s: Fail to get psy charger\n", __func__);
		return;
	}

	switch (rid) {
	case REG_RID_255K:
	case REG_RID_301K:
	case REG_RID_523K:
		val.intval = 1;
		break;
	default:
		val.intval = 0;
		break;
	}

	ret = psy_charger->desc->set_property(psy_charger,
			POWER_SUPPLY_EXT_PROP_FACTORY_MODE, &val);
	if (ret)
		s2mf301_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
}
#endif
static void s2mf301_notify_pdic_rid(struct s2mf301_usbpd_data *pdic_data, int rid)
{
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdic_data->is_factory_mode = false;
	if (rid == RID_523K)
		pdic_data->is_factory_mode = true;

#if IS_ENABLED(CONFIG_PDIC_MANUAL_QBAT) && !IS_ENABLED(CONFIG_SEC_FACTORY)
	s2mf301_manual_qbat_control(pdic_data, rid);
#endif
	/* rid */
	pdic_event_work(pd_data,
		PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_RID, rid/*rid*/, 0, 0);

	if (rid == REG_RID_523K || rid == REG_RID_619K || rid == REG_RID_OPEN) {
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB, 0/*attach*/, USB_STATUS_NOTIFY_DETACH, 0);
		pdic_data->is_host = HOST_OFF;
		pdic_data->is_client = CLIENT_OFF;
	} else if (rid == REG_RID_301K) {
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				1/*attach*/, USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
		pdic_data->is_host = HOST_OFF;
		pdic_data->is_client = CLIENT_ON;
	}
#else
	muic_attached_dev_t new_dev;

	pdic_data->is_factory_mode = false;
	switch (rid) {
	case REG_RID_255K:
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		break;
	case REG_RID_301K:
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		break;
	case REG_RID_523K:
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		pdic_data->is_factory_mode = true;
		break;
	case REG_RID_619K:
		new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		break;
	default:
		new_dev = ATTACHED_DEV_NONE_MUIC;
		return;
	}
	s2mf301_pdic_notifier_attach_attached_jig_dev(new_dev);
#endif
	dev_info(pdic_data->dev, "%s : attached rid state(%d)", __func__, rid);
}

static void s2mf301_usbpd_check_rid(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 rid;
	int prev_rid = pdic_data->rid;

	usleep_range(5000, 6000);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ADC_STATUS, &rid);
	rid = (rid & S2MF301_PDIC_RID_MASK) >> S2MF301_PDIC_RID_SHIFT;

	dev_info(pdic_data->dev, "%s : attached rid state(%d)", __func__, rid);

	if (rid) {
		if (pdic_data->rid != rid) {
			pdic_data->rid = rid;
			if (prev_rid >= REG_RID_OPEN && rid >= REG_RID_OPEN)
				dev_err(pdic_data->dev,
				  "%s : rid is not changed, skip notify(%d)", __func__, rid);
			else
				s2mf301_notify_pdic_rid(pdic_data, rid);
		}

		if (rid >= REG_RID_MAX) {
			dev_err(pdic_data->dev, "%s : overflow rid value", __func__);
			return;
		}
	}
}

int s2mf301_set_normal_mode(struct s2mf301_usbpd_data *pdic_data)
{
	u8 data_lpm;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data_lpm);
	data_lpm &= ~S2MF301_REG_LPM_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL, data_lpm);

	pdic_data->lpm_mode = false;

	s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_DRP);

	s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	dev_info(dev, "%s s2mf301 exit lpm mode, water_cc->DRP\n", __func__);

	return ret;
}

int s2mf301_usbpd_lpm_check(struct s2mf301_usbpd_data *pdic_data)
{
	u8 data_lpm = 0;
	struct i2c_client *i2c = pdic_data->i2c;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data_lpm);

	return (data_lpm & S2MF301_REG_LPM_EN);
}

void s2mf301_usbpd_set_mode(struct s2mf301_usbpd_data *pdic_data,
	PDIC_LPM_MODE_SEL mode)
{
	u8 data_lpm = 0;
	struct i2c_client *i2c = pdic_data->i2c;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data_lpm);
	if (mode == PD_LPM_MODE)
		data_lpm |= S2MF301_REG_LPM_EN;
	else if (mode == PD_NORMAL_MODE)
		data_lpm &= ~S2MF301_REG_LPM_EN;
	else {
		s2mf301_info("%s mode val(%d) is invalid\n", __func__, mode);
		return;
	}

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL, data_lpm);
}

static void s2mf301_usbpd_set_vbus_wakeup(struct s2mf301_usbpd_data *pdic_data,
	PDIC_VBUS_WAKEUP_SEL sel)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_TRIM, &data);
	if (sel == VBUS_WAKEUP_ENABLE)
		data &= ~S2MF301_REG_VBUS_WAKEUP_DIS;
	else if (sel == VBUS_WAKEUP_DISABLE)
		data |= S2MF301_REG_VBUS_WAKEUP_DIS;
	else {
		s2mf301_info("%s sel val(%d) is invalid\n", __func__, sel);
		return;
	}

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_TRIM, data);
}

int s2mf301_get_plug_monitor(struct s2mf301_usbpd_data *pdic_data, u8 *data)
{
	u8 reg_val;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;

	if (&data[0] == NULL || &data[1] == NULL) {
		s2mf301_err("%s NULL point data\n", __func__);
		return -1;
	}

	ret = s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &reg_val);
	if (ret < 0) {
		s2mf301_err("%s: S2MF301_REG_PLUG_MON1 Read err : %d\n",	__func__, ret);
		return ret;
	}

	data[0] = (reg_val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	data[1] = (reg_val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;
	s2mf301_info("%s, water pd mon pd1 : 0x%X, pd2 : 0x%X\n", __func__, data[0], data[1]);

	return ret;
}

int s2mf301_set_cable_detach_lpm_mode(struct s2mf301_usbpd_data *pdic_data)
{
	u8 data, data_lpm;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[S2MF301_MAX_NUM_INT_STATUS] = {0};

	pdic_data->lpm_mode = true;
	pdic_data->vbus_short_check_cnt = 0;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
	data &= ~(S2MF301_REG_PLUG_CTRL_MODE_MASK | S2MF301_REG_PLUG_CTRL_RP_SEL_MASK);
	data |= S2MF301_REG_PLUG_CTRL_DFP | S2MF301_REG_PLUG_CTRL_RP0;
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data_lpm);
	data_lpm |= S2MF301_REG_LPM_EN;

	s2mf301_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);

	ret = s2mf301_usbpd_bulk_read(i2c, S2MF301_REG_INT_STATUS0,
			S2MF301_MAX_NUM_INT_STATUS, intr);

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL, data_lpm);

	dev_info(dev, "%s enter.\n", __func__);

	return ret;
}

int s2mf301_set_lpm_mode(struct s2mf301_usbpd_data *pdic_data)
{
	u8 data_lpm, data2;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[S2MF301_MAX_NUM_INT_STATUS] = {0};

	pdic_data->lpm_mode = true;
	pdic_data->vbus_short_check_cnt = 0;
	s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_OPEN);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data_lpm);
	data_lpm |= S2MF301_REG_LPM_EN;

#if	(!IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_PDIC_MODE_BY_MUIC))
	s2mf301_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_DISABLE);
#endif

	s2mf301_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);

	ret = s2mf301_usbpd_bulk_read(i2c, S2MF301_REG_INT_STATUS0,
			S2MF301_MAX_NUM_INT_STATUS, intr);

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL, data_lpm);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &data2);
	data2 &=  ~S2MF301_REG_PLUG_CTRL_RpRd_MANUAL_MASK;
	data2 |= S2MF301_REG_PLUG_CTRL_RpRd_Rp_Source_Mode;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, data2);

	if (pdic_data->detach_valid == false) {
		s2mf301_usbpd_detach_init(pdic_data);
		s2mf301_usbpd_notify_detach(pdic_data);
	}

	dev_info(dev, "%s s2mf301 enter lpm mode, water_cc->CC_OPEN\n", __func__);

	return ret;
}

void _s2mf301_set_water_detect_pre_cond(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
	data &= ~(S2MF301_REG_PLUG_CTRL_MODE_MASK | S2MF301_REG_PLUG_CTRL_RP_SEL_MASK);
	data |= S2MF301_REG_PLUG_CTRL_DFP | S2MF301_REG_PLUG_CTRL_RP0;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data);
	data &= ~S2MF301_REG_LPM_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL, data);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_04, &data);
	data |= S2MF301_REG_PD1_RS_SW_ON_MASK | S2MF301_REG_PD2_RS_SW_ON_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ANALOG_OTP_04, data);

	msleep(300);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_04, &data);
	data &= ~(S2MF301_REG_OTP_PD_PUB_MASK | S2MF301_REG_PD_PU_LPM_CTRL_DIS_MASK
			| S2MF301_REG_PD1_RS_SW_ON_MASK | S2MF301_REG_PD2_RS_SW_ON_MASK);
	data |= S2MF301_REG_PD_PU_LPM_CTRL_DIS_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ANALOG_OTP_04, data);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_08, &data);
	data &= ~S2MF301_REG_LPMPUI_SEL_MASK;
	data |= S2MF301_REG_LPMPUI_SEL_1UA_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ANALOG_OTP_08, data);
}

void _s2mf301_set_water_detect_post_cond(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_04, &data);
	data &= ~(S2MF301_REG_OTP_PD_PUB_MASK | S2MF301_REG_PD_PU_LPM_CTRL_DIS_MASK);
	data |= S2MF301_REG_OTP_PD_PUB_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ANALOG_OTP_04, data);
}

void s2mf301_usbpd_set_cc_state(struct s2mf301_usbpd_data *pdic_data, int cc)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data;

	s2mf301_info("%s, cur:%s -> next:%s\n", __func__, s2m_cc_state_str[pdic_data->cc_state],
			s2m_cc_state_str[cc]);

	if (pdic_data->is_manual_cc_open) {
		s2mf301_info("%s, CC_OPEN(0x%x)\n", __func__, pdic_data->is_manual_cc_open);
		cc = CC_STATE_OPEN;
	}
	if (pdic_data->rid != REG_RID_UNDF && pdic_data->rid != REG_RID_MAX) {
		s2mf301_info("%s, skip rid(0x%x) is attached\n", __func__, pdic_data->rid);
		return;
	}

	if (pdic_data->cc_state == cc)
		return;

	pdic_data->cc_state = cc;
	switch(cc) {
	case CC_STATE_OPEN:
		/* set Rp + 0uA */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
		data &= ~(S2MF301_REG_PLUG_CTRL_MODE_MASK | S2MF301_REG_PLUG_CTRL_RP_SEL_MASK);
		data |= S2MF301_REG_PLUG_CTRL_DFP | S2MF301_REG_PLUG_CTRL_RP0;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
		/* manual off */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &data);
		data &=  ~S2MF301_REG_PLUG_CTRL_RpRd_MANUAL_MASK;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, data);
		break;
	case CC_STATE_RD:
		/* manual Rd */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &data);
		data &=  ~S2MF301_REG_PLUG_CTRL_RpRd_MANUAL_MASK;
		data |= S2MF301_REG_PLUG_CTRL_RpRd_Rd_Sink_Mode;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, data);
		/* set Rd / Rp is ignored */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
		data &= ~(S2MF301_REG_PLUG_CTRL_MODE_MASK | S2MF301_REG_PLUG_CTRL_RP_SEL_MASK);
		data |= S2MF301_REG_PLUG_CTRL_UFP | S2MF301_REG_PLUG_CTRL_RP80;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
		break;
	case CC_STATE_DRP:
		/* manual off(DRP) */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &data);
		data &=  ~S2MF301_REG_PLUG_CTRL_RpRd_MANUAL_MASK;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, data);
		/* DRP, 80uA default */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &data);
		data &= ~(S2MF301_REG_PLUG_CTRL_MODE_MASK | S2MF301_REG_PLUG_CTRL_RP_SEL_MASK);
		data |= S2MF301_REG_PLUG_CTRL_DRP | S2MF301_REG_PLUG_CTRL_RP80;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, data);
		break;
	default:
		break;
	}
}

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
static void s2mf301_usbpd_check_facwater(struct work_struct *work)
{
	struct s2mf301_usbpd_data *pdic_data =
		container_of(work, struct s2mf301_usbpd_data, check_facwater.work);
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct power_supply *psy_pm = pdic_data->psy_pm;
	int ret = 0;


	if (pdic_data->facwater_check_cnt >= 14) {
		/* until 7sec */
		goto done;
	} else if (!psy_pm) {
		s2mf301_err("%s, Fail to get psy_pm\n", __func__);
		goto done;
	}

	pdic_data->facwater_check_cnt++;
	s2mf301_info("%s cnt(%d)\n", __func__, pdic_data->facwater_check_cnt);

	msleep(100);
	ret = s2mf301_water_check_facwater(&pdic_data->water);

	if (ret) {
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MANAGER,
			PDIC_NOTIFY_ID_WATER, 1, 0, 0);
		cancel_delayed_work(&pdic_data->check_facwater);
	} else {
		cancel_delayed_work(&pdic_data->check_facwater);
		schedule_delayed_work(&pdic_data->check_facwater, msecs_to_jiffies(500));
	}

done:
	return;
}

void s2mf301_usbpd_water_set_status(struct s2mf301_usbpd_data *pdic_data, int status)
{
#if IS_ENABLED(CONFIG_USB_HW_PARAM) && !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	struct usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);

	if (status > S2M_WATER_STATUS_WATER) {
		s2mf301_info("[WATER] %s, status invalid(%d)\n", __func__, status);
	} else {
		s2mf301_info("[WATER] %s cur:%s -> next:%s\n", __func__,
			WATER_STATUS_TO_STR[pdic_data->water_status], WATER_STATUS_TO_STR[status]);
	}

	pdic_data->water_status = status;
	switch (status) {
	case S2M_WATER_STATUS_DRY:
		s2mf301_info("%s, PDIC DRY detected\n", __func__);
		pdic_data->is_water_detect = false;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MANAGER,
			PDIC_NOTIFY_ID_WATER, 0, 0, 0);
#endif
		s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_DRP);
		msleep(50);
		s2mf301_set_irq_enable(pdic_data,
				ENABLED_INT_0, ENABLED_INT_1, ENABLED_INT_2,
				ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
		break;
	case S2M_WATER_STATUS_WATER:
		s2mf301_info("%s, PDIC WATER detected\n", __func__);
		pdic_data->is_water_detect = true;
	s2mf301_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);
	usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_OFF);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_USB,
			PDIC_NOTIFY_ID_USB, 0/*attach*/, USB_STATUS_NOTIFY_DETACH, 0);
		if (pdic_data->power_off_water_detected) {
			pdic_data->power_off_water_detected = 0;
			pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MANAGER,
				PDIC_NOTIFY_ID_POFF_WATER, 1, 0, 0);
		}
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MANAGER,
			PDIC_NOTIFY_ID_WATER, 1, 0, 0);
#endif
#if IS_ENABLED(CONFIG_USB_HW_PARAM) && !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_WATER_INT_COUNT);
#endif
		s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_RD);
		break;
	default:
		break;
	}
}
#endif

static void s2mf301_usbpd_otg_attach(struct s2mf301_usbpd_data *pdic_data)
{
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);

	if (pdic_data->detach_valid || pdic_data->power_role == PDIC_SINK) {
		s2mf301_info("%s, detach(%d), pr(%d) return\n", __func__,
				pdic_data->detach_valid, pdic_data->power_role);
		goto out;
	}

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER) && !IS_ENABLED(CONFIG_SEC_FACTORY)
	if (pdic_data->is_killer == true || pdic_data->is_water_detect == true) {
		s2mf301_info("%s, killer(%d), water(%d) return\n", __func__,
				pdic_data->is_killer, pdic_data->is_water_detect);
		goto out;
	}
#endif

	/* otg */
	pdic_data->is_host = HOST_ON;
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
	pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
#elif IS_ENABLED(CONFIG_TYPEC)
	pd_data->typec_power_role = TYPEC_SOURCE;
	typec_set_pwr_role(pd_data->port, pd_data->typec_power_role);
#endif
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 1);
#endif

	/* USB */
	pdic_event_work(pd_data, PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
			1/*attach*/, USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/, 0);
	/* add to turn on external 5V */
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	if (!is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST)) {
#if IS_ENABLED(CONFIG_PM_S2MF301)
		s2mf301_usbpd_check_vbus(pdic_data, 800, VBUS_OFF);
#endif
		usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_ON);
	}
#endif
	usbpd_manager_acc_handler_cancel(dev);
out:
#if IS_ENABLED(CONFIG_ARCH_QCOM)
	__pm_relax(pdic_data->water_wake);
#endif
	return;
}

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
static int type3_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	PD_NOTI_ATTACH_TYPEDEF *p_noti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
	struct s2mf301_usbpd_data *pdic_data =
		container_of(nb, struct s2mf301_usbpd_data,
			     type3_nb);
	struct usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
#if !IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_USB_HOST_NOTIFY) && \
	(IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF) || IS_ENABLED(CONFIG_TYPEC))
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0;
#endif

#if (IS_ENABLED(CONFIG_USB_HW_PARAM) && !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)) || \
	(!IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_USB_HOST_NOTIFY))
	struct otg_notify *o_notify = get_otg_notify();
#endif
	mutex_lock(&pdic_data->lpm_mutex);
	s2mf301_info("%s action:%d, attached_dev:%d, lpm:%d, pdic_data->is_otg_vboost:%d, pdic_data->is_otg_reboost:%d\n",
		__func__, (int)action, (int)attached_dev, pdic_data->lpm_mode,
		(int)pdic_data->is_otg_vboost, (int)pdic_data->is_otg_reboost);

	if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		(attached_dev == ATTACHED_DEV_TYPE3_MUIC)) {
		pdic_data->is_muic_water_detect = false;
		if (pdic_data->lpm_mode) {
			s2mf301_info("%s try to exit lpm mode-->\n", __func__);
			s2mf301_set_normal_mode(pdic_data);
			s2mf301_info("%s after exit lpm mode<--\n", __func__);
		}
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		attached_dev == ATTACHED_DEV_ABNORMAL_OTG_MUIC) {
		pdic_data->is_killer = true;
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		attached_dev == ATTACHED_DEV_OTG_MUIC) {
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER) && !IS_ENABLED(CONFIG_SEC_FACTORY)
		mutex_lock(&pdic_data->s2m_water_mutex);
#endif
		s2mf301_usbpd_otg_attach(pdic_data);
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER) && !IS_ENABLED(CONFIG_SEC_FACTORY)
		mutex_unlock(&pdic_data->s2m_water_mutex);
#endif
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_DETACH) &&
		attached_dev == ATTACHED_DEV_UNDEFINED_RANGE_MUIC) {
		s2mf301_info("%s, DETACH : ATTACHED_DEV_UNDEFINED_RANGE_MUIC(Water DRY)\n", __func__);
		//s2mf301_set_cable_detach_lpm_mode(pdic_data);
#if !IS_ENABLED(CONFIG_PDIC_MODE_BY_MUIC)
		s2mf301_set_normal_mode(pdic_data);
#endif
#if IS_ENABLED(CONFIG_USB_HW_PARAM) && !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_DRY_INT_COUNT);
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_WATER, PDIC_NOTIFY_DETACH, 0, 0);
#endif
		msleep(50);
		s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
		msleep(50);
		pdic_data->is_muic_water_detect = false;
	} else if (action == MUIC_PDIC_NOTIFY_CMD_DETACH) {
		if (!pdic_data->lpm_mode) {
			s2mf301_info("%s try to enter lpm mode-->\n", __func__);
			s2mf301_set_lpm_mode(pdic_data);
			s2mf301_info("%s after enter lpm mode<--\n", __func__);
		}
	}
#if !IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_USB_HOST_NOTIFY) && \
	(IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF) || IS_ENABLED(CONFIG_TYPEC))
	else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH)
			&& (attached_dev == ATTACHED_DEV_CHECK_OCP)
			&& pdic_data->is_otg_vboost
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
			&& pdic_data->data_role_dual == USB_STATUS_NOTIFY_ATTACH_DFP
#elif IS_ENABLED(CONFIG_TYPEC)
			&& pd_data->typec_data_role == TYPEC_HOST
#endif
	) {
		if (o_notify) {
			if (is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST)) {
				s2mf301_info("%s, upsm mode, skip OCP handling\n", __func__);
				goto EOH;
			}
		}
		if (pdic_data->is_otg_reboost) {
			/* todo : over current event to platform */
			s2mf301_info("%s, CHECK_OCP, Can't afford it(OVERCURRENT)\n", __func__);
			if (o_notify)
				send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
			goto EOH;
		}
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH, 1/*attach*/, 1/*rprd*/, 0);

		s2mf301_info("%s, CHECK_OCP, start OCP W/A\n", __func__);
		pdic_data->is_otg_reboost = true;
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD_HOLD, &reg_data);
		reg_data |= S2MF301_REG_PLUG_CTRL_PD_HOLD_BIT;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD_HOLD, reg_data);

		s2mf301_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP80);
		usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_OFF);
		usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_ON);

		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD_HOLD, &reg_data);
		reg_data &= ~S2MF301_REG_PLUG_CTRL_PD_HOLD_BIT;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD_HOLD, reg_data);
	}
EOH:
#endif
	mutex_unlock(&pdic_data->lpm_mutex);

	return 0;
}
#endif

static void s2mf301_vbus_short_check(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = pdic_data->dev;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
#endif
	u8 val = 0;
	u8 cc1_val = 0, cc2_val = 0;
	u8 rp_currentlvl = 0;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
	int event = 0;
#endif

	if (pdic_data->vbus_short_check)
		return;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_FSM_MON, &val);

	cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

	dev_info(dev, "%s, 10k check : cc1_val(%x), cc2_val(%x)\n",
					__func__, cc1_val, cc2_val);

	if (cc1_val == USBPD_10k || cc2_val == USBPD_10k)
		rp_currentlvl = RP_CURRENT_LEVEL3;
	else if (cc1_val == USBPD_22k || cc2_val == USBPD_22k)
		rp_currentlvl = RP_CURRENT_LEVEL2;
	else if (cc1_val == USBPD_56k || cc2_val == USBPD_56k)
		rp_currentlvl = RP_CURRENT_LEVEL_DEFAULT;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);

	cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

	dev_info(dev, "%s, vbus short check : cc1_val(%x), cc2_val(%x)\n",
					__func__, cc1_val, cc2_val);

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	mutex_lock(&pdic_data->water.mutex);
	s2mf301_usbpd_get_gpadc_volt(pdic_data);
	s2mf301_info("%s, vGPADC(%dmV), th(%dmV)\n", __func__,
			pdic_data->pm_vgpadc, S2MF301_WATER_GPADC_SHORT);
	mutex_unlock(&pdic_data->water.mutex);
#endif
	if (cc1_val == USBPD_Rp || cc2_val == USBPD_Rp
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
			|| PD_GPADC_SHORT(pdic_data->pm_vgpadc)
#endif
			) {
		s2mf301_info("%s, Vbus short\n", __func__);
		pdic_data->vbus_short = true;
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
		if (o_notify) {
			event = NOTIFY_EXTRA_SYSMSG_CC_SHORT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
		}
#endif
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_VBUS_CC_SHORT_COUNT);
#endif
	} else {
		s2mf301_info("%s, Vbus not short\n", __func__);
		pdic_data->vbus_short = false;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		pd_data->pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
		pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PDIC_ATTACH;
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_BATT, PDIC_NOTIFY_ID_POWER_STATUS, 0, 0, 0);
		if (rp_currentlvl == RP_CURRENT_LEVEL_DEFAULT)
			pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
				PDIC_NOTIFY_ID_TA, 1/*attach*/, 0/*rprd*/, 0);
#endif
	}

	pdic_data->vbus_short_check = true;
}

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
static int s2mf301_power_off_water_check(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val, prev_val, data_lpm = 0;
	u8 cc1_val, cc2_val;
	int retry = 0;
	int ret = true;
	u8 rid = 0;

	mutex_lock(&pdic_data->_mutex);
	mutex_lock(&pdic_data->lpm_mutex);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ADC_STATUS, &rid);
	rid = (rid & S2MF301_PDIC_RID_MASK) >> S2MF301_PDIC_RID_SHIFT;
	s2mf301_info("%s, rid (0x%x)\n", __func__, rid);

	switch (rid) {
	case REG_RID_255K:
	case REG_RID_301K:
	case REG_RID_523K:
	case REG_RID_619K:
		s2mf301_info("%s, Skip by CC_RID\n", __func__);
		goto exit;
	default:
		break;
	}

	pdic_data->power_off_water_detected = 0;
	pdic_data->water_gpadc_short = 2500;
	pdic_data->water_gpadc_poweroff = 1500;
	pdic_data->water_status = S2M_WATER_STATUS_INVALID;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &val);
	prev_val = val;
	val &= ~(S2MF301_REG_PLUG_CTRL_MODE_MASK | S2MF301_REG_PLUG_CTRL_RP_SEL_MASK);
	val |= S2MF301_REG_PLUG_CTRL_RP0 | S2MF301_REG_PLUG_CTRL_DRP;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, val);

	if (pdic_data->lpm_mode) {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data_lpm);
		data_lpm &= ~S2MF301_REG_LPM_EN;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL, data_lpm);
	}

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
	val |= S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);
	msleep(50);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SRC;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

	usleep_range(1000, 1100);

	for (retry = 0; retry < 3; retry++) {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);

		cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
		cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

		s2mf301_info("%s, vbus short check(%d) : cc1_val(%x), cc2_val(%x)\n",
						__func__, retry, cc1_val, cc2_val);

		if (cc1_val == USBPD_Ra || cc2_val == USBPD_Ra)
			break;
		else if (retry == 2) {
			ret = false;
			/* Some TA show vRp on both CC pin */
			//pdic_data->power_off_water_detected = 1;
		}
		udelay(5);
	}

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, prev_val);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, val);

	if (pdic_data->lpm_mode) {
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL, &data_lpm);
		data_lpm |= S2MF301_REG_LPM_EN;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL, data_lpm);
	}

	s2mf301_usbpd_get_gpadc_volt(pdic_data);
	s2mf301_info("%s, ret(%d), gpadc(%d)\n", __func__, ret, pdic_data->pm_vgpadc);
	if (pdic_data->power_off_water_detected == false) {
		if (pdic_data->pm_vgpadc >= pdic_data->water_gpadc_poweroff) {
			s2mf301_info("%s, SBU water detected\n", __func__);
			ret = false;
			pdic_data->power_off_water_detected = 1;
		}
	}

	s2mf301_info("%s, water(%d)\n", __func__, pdic_data->power_off_water_detected);

exit:
	mutex_unlock(&pdic_data->lpm_mutex);
	mutex_unlock(&pdic_data->_mutex);

	return ret;
}

static void s2mf301_power_off_water_notify(struct s2mf301_usbpd_data *pdic_data)
{
	int pm_ret = 0;
	union power_supply_propval value;

	mutex_lock(&pdic_data->_mutex);
	mutex_lock(&pdic_data->lpm_mutex);

	pdic_data->water.event = S2M_WATER_EVENT_WATER_DETECTED;
	s2mf301_water_init_work_start(&pdic_data->water);
	//s2mf301_usbpd_water_set_status(pdic_data, S2M_WATER_STATUS_WATER);
	//power_supply_set_property(pdic_data->psy_pm,
	//	(enum power_supply_property)POWER_SUPPLY_LSI_PROP_WATER_STATUS, &value);
	msleep(200);
	pdic_data->psy_muic = get_power_supply_by_name("muic-manager");
	pm_ret = s2mf301_usbpd_get_pmeter_volt(pdic_data);
	s2mf301_info("%s, notify PowerOffWater, vchgin(%d)\n", __func__, pdic_data->pm_chgin);

	if (!pm_ret && (pdic_data->pm_chgin >= 4000) && pdic_data->psy_muic) {
		value.intval = 1;
		power_supply_set_property(pdic_data->psy_muic,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_HICCUP_MODE, &value);
	}
	msleep(200);

	mutex_unlock(&pdic_data->lpm_mutex);
	mutex_unlock(&pdic_data->_mutex);
}
#endif

static void s2mf301_vbus_dischg_off_work(struct work_struct *work)
{
	struct s2mf301_usbpd_data *pdic_data =
		container_of(work, struct s2mf301_usbpd_data, vbus_dischg_off_work.work);

	if (gpio_is_valid(pdic_data->vbus_dischg_gpio)) {
		gpio_direction_output(pdic_data->vbus_dischg_gpio, 0);
		s2mf301_info("%s vbus_discharging(%d)\n", __func__,
				gpio_get_value(pdic_data->vbus_dischg_gpio));
	}
}

static void s2mf301_usbpd_set_vbus_dischg_gpio(struct s2mf301_usbpd_data
		*pdic_data, int vbus_dischg)
{
	if (!gpio_is_valid(pdic_data->vbus_dischg_gpio))
		return;

	cancel_delayed_work_sync(&pdic_data->vbus_dischg_off_work);

	gpio_direction_output(pdic_data->vbus_dischg_gpio, vbus_dischg);
	s2mf301_info("%s vbus_discharging(%d)\n", __func__,
			gpio_get_value(pdic_data->vbus_dischg_gpio));

	if (vbus_dischg > 0)
		schedule_delayed_work(&pdic_data->vbus_dischg_off_work,
				msecs_to_jiffies(120));
}

static void s2mf301_usbpd_detach_init(struct s2mf301_usbpd_data *pdic_data)
{
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_S2MF301_PDIC_SUPPORT_S2MC501)
	struct usbpd_manager_data *manager = &pd_data->manager;
#endif
	struct i2c_client *i2c = pdic_data->i2c;
	int ret = 0;
	u8 rid = 0;
	u8 reg_data = 0;

	dev_info(dev, "%s\n", __func__);

	mutex_lock(&pdic_data->pd_mutex);
	s2mf301_usbpd_check_pps_irq(pd_data, false);
	s2mf301_usbpd_check_pps_irq_reduce_clk(pd_data, false);
	s2mf301_usbpd_set_vbus_dischg_gpio(pdic_data, 1);
	s2mf301_usbpd_set_pd_control(pdic_data, USBPD_CC_OFF);
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
	if (pdic_data->power_role_dual == DUAL_ROLE_PROP_PR_SRC)
		usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_OFF);

#elif IS_ENABLED(CONFIG_TYPEC)
	if (pd_data->typec_power_role == TYPEC_SOURCE)
		usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_OFF);

	pd_data->pd_support = 0;
#endif
	s2mf301_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP80);
	pdic_data->detach_valid = true;
	mutex_unlock(&pdic_data->pd_mutex);

	usbpd_manager_plug_detach(dev, 0);

#if IS_ENABLED(CONFIG_S2MF301_PDIC_SUPPORT_S2MC501)
	mutex_lock(&manager->pdo_mutex);
	s2mf301_pps_enable(pd_data, 0);
	mutex_unlock(&manager->pdo_mutex);
#endif

	/* wait flushing policy engine work */
	usbpd_cancel_policy_work(dev);

	pdic_data->status_reg[0] = pdic_data->status_reg[1] = 0;
	usbpd_reinit(dev);
	/* for pdic hw detect */
	ret = s2mf301_usbpd_write_reg(i2c, S2MF301_REG_MSG_SEND_CON, S2MF301_REG_MSG_SEND_CON_HARD_EN);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ADC_STATUS, &rid);
	rid = (rid & S2MF301_PDIC_RID_MASK) >> S2MF301_PDIC_RID_SHIFT;
	if (!rid) {
		s2mf301_self_soft_reset(pdic_data);
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, &reg_data);
		if ((reg_data & S2MF301_REG_PLUG_CTRL_MODE_MASK) != S2MF301_REG_PLUG_CTRL_DRP) {
			if (pdic_data->is_manual_cc_open)
				s2mf301_info("%s, CC_OPEN(0x%x)\n", __func__, pdic_data->is_manual_cc_open);
			else {
				reg_data |= S2MF301_REG_PLUG_CTRL_DRP;
				s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PORT, reg_data);
			}
		}
	}
	s2mf301_snk(i2c);
    s2mf301_ufp(i2c);
	pdic_data->rid = REG_RID_MAX;
	pdic_data->is_factory_mode = false;
	pdic_data->is_pr_swap = false;
	pdic_data->vbus_short_check = false;
	pdic_data->pd_vbus_short_check = false;
	pdic_data->vbus_short = false;
	pdic_data->is_killer = false;
	pdic_data->first_goodcrc = 0;
	pdic_data->cc_instead_of_vbus = 0;
	pd_data->pps_pd = 0;

	if (pdic_data->regulator_en)
		ret = regulator_disable(pdic_data->regulator);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	pd_data->pd_noti.sink_status.current_pdo_num = 0;
	pd_data->pd_noti.sink_status.selected_pdo_num = 0;
	pd_data->pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
#endif
	s2mf301_usbpd_reg_init(pdic_data);
	s2mf301_set_vconn_source(pd_data, USBPD_VCONN_OFF);
}

static void s2mf301_usbpd_notify_detach(struct s2mf301_usbpd_data *pdic_data)
{
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	/* MUIC */
	pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
							0/*attach*/, 0/*rprd*/, 0);

	pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_RID,
							REG_RID_OPEN/*rid*/, 0, 0);

	usbpd_manager_acc_detach(dev);
	if (pdic_data->is_host > HOST_OFF || pdic_data->is_client > CLIENT_OFF) {
		/* usb or otg */
		dev_info(dev, "%s %d: is_host = %d, is_client = %d\n", __func__,
				__LINE__, pdic_data->is_host, pdic_data->is_client);
		pdic_data->is_host = HOST_OFF;
		pdic_data->is_client = CLIENT_OFF;
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif IS_ENABLED(CONFIG_TYPEC)
		pd_data->typec_power_role = TYPEC_SINK;
		pd_data->typec_data_role = TYPEC_DEVICE;
#endif
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
		send_otg_notify(o_notify, NOTIFY_EVENT_DR_SWAP, 0);
#endif
		/* USB */
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
					0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		/* Standard Vendor ID */
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_ALL,
					PDIC_NOTIFY_ID_CLEAR_INFO, PDIC_NOTIFY_ID_SVID_INFO, 0, 0);
	}
#else
	usbpd_manager_plug_detach(dev, 1);
#endif
}

#if defined(CONFIG_S2MF301_PDIC_TRY_SNK)
static enum alarmtimer_restart s2mf301_usbpd_try_snk_alarm_srcdet(struct alarm *alarm, ktime_t now)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(alarm, struct s2mf301_usbpd_data, srcdet_alarm);

	s2mf301_info("%s, ++\n", __func__);
	pdic_data->srcdet_expired = 1;

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart s2mf301_usbpd_try_snk_alarm_snkdet(struct alarm *alarm, ktime_t now)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(alarm, struct s2mf301_usbpd_data, snkdet_alarm);

	s2mf301_info("%s, ++\n", __func__);
	pdic_data->snkdet_expired = 1;

	return ALARMTIMER_NORESTART;
}

static void s2mf301_usbpd_try_snk(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);

	u8 intr[S2MF301_MAX_NUM_INT_STATUS] = {0, };

	bool is_src_detected = 0;
	bool is_snk_detected = 0;
	bool vbus_detected = 0;
	int vbus;
	bool power_role = 0;
	u8 manual, fsm, val;
	u8 cc1, cc2;

	ktime_t sec, msec;
	s64 start = 0, duration = 0;

	s2mf301_info("%s, ++\n", __func__);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &fsm);
	fsm &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	fsm |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, fsm);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &manual);
	manual |= S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, manual);

	usbpd_timer1_start(pd_data);

	pdic_data->snkdet_expired = 0;
	pdic_data->srcdet_expired = 0;

	start = ktime_to_us(ktime_get());

	usleep_range(75000, 75100); /* wait tDRPtry */

	while (1) {
		duration = ktime_to_us(ktime_get()) - start;

#if IS_ENABLED(CONFIG_PM_S2MF301)
		s2mf301_usbpd_get_pmeter_volt(pdic_data);
#endif
		/* vbus is over 4000 or fail to get_prop */
		vbus = pdic_data->pm_chgin;
		vbus_detected = (vbus < 0) ? true : ((vbus >= 4000) ? true : false);

		/* Source not Detected for tTryCCDebounce after tDRPTry */
		if (is_snk_detected && pdic_data->snkdet_expired) {
			s2mf301_info("%s, sink not detected, goto TryWait.SRC\n", __func__);
			power_role = 1;
			break;
		}

		/* Source Detected for tTryCCDebounce and VBUS Detected */
		if ((is_src_detected && pdic_data->srcdet_expired && vbus_detected)
				|| (duration > 1500 * USEC_PER_MSEC)) { /* Breaking out of an infinite loop */
			s2mf301_info("%s, sink detected, goto Attached.SNK\n", __func__);
			power_role = 0;
			break;
		}

		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);
		cc1 = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
		cc2 = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

		if ((cc1 == USBPD_Rd) || (cc2 == USBPD_Rd)) {
			//Src detected
			if (!is_src_detected) {
				is_src_detected = 1;
				alarm_cancel(&pdic_data->srcdet_alarm);
				pdic_data->srcdet_expired = 0;
				sec = ktime_get_boottime();
				msec = ktime_set(0, tTryCCDebounce * NSEC_PER_MSEC);
				alarm_start(&pdic_data->srcdet_alarm, ktime_add(sec, msec));
			}
			is_snk_detected = 0;
			alarm_cancel(&pdic_data->snkdet_alarm);
		} else {
			//Src not detected
			if (!is_snk_detected) {
				is_snk_detected = 1;
				alarm_cancel(&pdic_data->snkdet_alarm);
				pdic_data->snkdet_expired = 0;
				sec = ktime_get_boottime();
				msec = ktime_set(0, tTryCCDebounce * NSEC_PER_MSEC);
				alarm_start(&pdic_data->snkdet_alarm, ktime_add(sec, msec));
			}
			is_src_detected = 0;
			alarm_cancel(&pdic_data->srcdet_alarm);

		}
		usleep_range(1000, 1100);
	}

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, &fsm);
	fsm &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	if (power_role)
		/* goto TryWait.SRC */
		fsm |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SRC;
	else
		/* goto Attached.SNK */
		fsm |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, fsm);

	if (power_role)
		start = ktime_to_us(ktime_get());

	usleep_range(3000, 3100);

	if (power_role) {
		is_snk_detected = 0;
		while (1) {
			duration = ktime_to_us(ktime_get()) - start;

			s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);
			cc1 = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
			cc2 = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

			if ((cc1 == USBPD_Rd) || (cc2 == USBPD_Rd)) {
				/* Snk detected */
				if (duration > tTryCCDebounce * USEC_PER_MSEC) {
					s2mf301_info("%s, goto Attached.SRC\n", __func__);
					fsm &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
					fsm |= S2MF301_REG_PLUG_CTRL_FSM_ATTACHED_SRC;
					s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, fsm);
					s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &manual);
					manual &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
					s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, manual);
					/* Snk Detected for tTryCCDebounce */
					/* Attached.SRC -> Attach */
					break;
				}
			} else {
				/* Snk Not Detected */
				/* Attached.SRC -> need Detach */
				if (duration > tDRPtry * USEC_PER_MSEC) {
					s2mf301_info("%s, goto Unattached.SNK\n", __func__);

					/* make detach in Unattached.SRC */
					fsm &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
					fsm |= S2MF301_REG_PLUG_CTRL_FSM_UNATTACHED_SRC;
					s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD12, fsm);

					/* disable manual mode */
					s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &manual);
					manual &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
					s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, manual);
					msleep(20); /* Waitting for updating monitor reg by HW*/
					return;
				}
			}
			usleep_range(1000, 1100);
		}
	}
	usleep_range(1000, 1100);

	s2mf301_usbpd_bulk_read(i2c, S2MF301_REG_INT_STATUS0, S2MF301_MAX_NUM_INT_STATUS, intr);
	s2mf301_info("%s, status[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]\n",
			__func__, intr[0], intr[1], intr[2], intr[3], intr[4],
			intr[5], intr[6], intr[7], intr[8], intr[9]);
}
#endif

static void s2mf301_usbpd_check_host(struct s2mf301_usbpd_data *pdic_data,
							PDIC_HOST_REASON host)
{
	struct usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	if (host == HOST_ON && pdic_data->is_host == HOST_ON) {
		dev_info(pdic_data->dev, "%s %d: turn off host\n", __func__, __LINE__);
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
				PDIC_NOTIFY_ID_ATTACH, 0/*attach*/, 1/*rprd*/, 0);
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif IS_ENABLED(CONFIG_TYPEC)
		pd_data->typec_power_role = TYPEC_SINK;
		typec_set_pwr_role(pd_data->port, pd_data->typec_power_role);
#endif
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
		/* add to turn off external 5V */
		usbpd_manager_vbus_turn_on_ctrl(pd_data, VBUS_OFF);

		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
					0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		pdic_data->is_host = HOST_OFF;
		msleep(300);
	} else if (host == HOST_OFF && pdic_data->is_host == HOST_OFF) {
		/* muic */
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
				PDIC_NOTIFY_ID_OTG, 1/*attach*/, 0/*rprd*/, 0);
#if IS_ENABLED(CONFIG_ARCH_QCOM)
		cancel_delayed_work(&pdic_data->water_wake_work);
		schedule_delayed_work(&pdic_data->water_wake_work, msecs_to_jiffies(1000));
#endif
	}
}

static void s2mf301_usbpd_check_client(struct s2mf301_usbpd_data *pdic_data,
							PDIC_DEVICE_REASON client)
{
	struct usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	if (client == CLIENT_ON && pdic_data->is_client == CLIENT_ON) {
		dev_info(pdic_data->dev, "%s %d: turn off client\n", __func__, __LINE__);
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_MUIC,
				PDIC_NOTIFY_ID_ATTACH, 0/*attach*/, 0/*rprd*/, 0);
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif IS_ENABLED(CONFIG_TYPEC)
		pd_data->typec_power_role = TYPEC_SINK;
		typec_set_pwr_role(pd_data->port, pd_data->typec_power_role);
#endif
		pdic_event_work(pd_data, PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
					0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		pdic_data->is_client = CLIENT_OFF;
	}
}

static int s2mf301_check_port_detect(struct s2mf301_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	u8 data, val;
	u8 cc1_val = 0, cc2_val = 0;
	int ret = 0;
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER) && !IS_ENABLED(CONFIG_SEC_FACTORY)
	int timeout = 0;
#endif

	ret = s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON2, &data);
	if (ret < 0)
		dev_err(dev, "%s, i2c read PLUG_MON2 error\n", __func__);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);

	cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
	cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

	pdic_data->cc1_val = cc1_val;
	pdic_data->cc2_val = cc2_val;

	dev_info(dev, "%s, attach pd pin check cc1_val(%x), cc2_val(%x)\n",
					__func__, cc1_val, cc2_val);
#if IS_ENABLED(CONFIG_S2M_PDIC_MANUAL_RETRY)
	s2mf301_ops_set_manual_retry(pd_data, 0);
#endif

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER) && !IS_ENABLED(CONFIG_SEC_FACTORY)
	if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SINK) {
		dev_info(dev, "SINK\n");
		pdic_data->water.event = S2M_WATER_EVENT_ATTACH_AS_SNK;
	} else if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SOURCE) {
		dev_info(dev, "SOURCE\n");
		pdic_data->water.event = S2M_WATER_EVENT_ATTACH_AS_SRC;
	} else {
		dev_err(dev, "%s, PLUG Error\n", __func__);
		if (pdic_data->water.cur_state == S2M_WATER_STATE_WAIT)
			complete(&pdic_data->water.water_wait_compl);
		return -1;
	}
	if (pdic_data->water.cur_state == S2M_WATER_STATE_WAIT)
		complete(&pdic_data->water.water_wait_compl);

	schedule_delayed_work(&pdic_data->water.state_work, 0);

	reinit_completion(&pdic_data->water.water_check_done);
	timeout = wait_for_completion_timeout(&pdic_data->water.water_check_done,
			msecs_to_jiffies(600));
	if (timeout == 0)
		s2mf301_info("%s, watercheck timeout\n", __func__);

	if (pdic_data->water.status == S2M_WATER_STATUS_WATER) {
		s2mf301_info("%s, Water Detected, skip attach\n", __func__);
		return -1;
	}
#endif

#if defined(CONFIG_S2MF301_PDIC_TRY_SNK)
	if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SOURCE) {
		/* if AudioAcc Support, trans to Audio from AttachWait.Src */
		ret = s2mf301_usbpd_check_accessory(pdic_data);
		if (ret < 0) {
			ret = -1;
			goto out;
		}

		s2mf301_usbpd_try_snk(pdic_data);
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON2, &data);
		s2mf301_info("%s, after try.snk data = %x\n", __func__, data);

		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON1, &val);

		cc1_val = (val & S2MF301_REG_CTRL_MON_PD1_MASK) >> S2MF301_REG_CTRL_MON_PD1_SHIFT;
		cc2_val = (val & S2MF301_REG_CTRL_MON_PD2_MASK) >> S2MF301_REG_CTRL_MON_PD2_SHIFT;

		pdic_data->cc1_val = cc1_val;
		pdic_data->cc2_val = cc2_val;

		dev_info(dev, "%s, attach pd pin check cc1_val(%x), cc2_val(%x)\n",
					__func__, cc1_val, cc2_val);
	}
#endif

	if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SINK) {
		dev_info(dev, "SINK\n");
		s2mf301_usbpd_set_vbus_dischg_gpio(pdic_data, 0);
		pdic_data->detach_valid = false;
		pdic_data->power_role = PDIC_SINK;
		pdic_data->data_role = USBPD_UFP;
		s2mf301_snk(i2c);
		s2mf301_ufp(i2c);
		usbpd_policy_reset(pd_data, PLUG_EVENT);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		dev_info(&i2c->dev, "%s %d: is_host = %d, is_client = %d\n", __func__,
					__LINE__, pdic_data->is_host, pdic_data->is_client);
		if (pdic_data->regulator_en) {
			ret = regulator_enable(pdic_data->regulator);
			if (ret)
				dev_err(&i2c->dev, "Failed to enable vconn LDO: %d\n", ret);
		}

		s2mf301_usbpd_check_host(pdic_data, HOST_ON);
		/* muic */
		pdic_event_work(pd_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH, 1/*attach*/, 0/*rprd*/, 0);
		if (!(pdic_data->rid == REG_RID_523K || pdic_data->rid == REG_RID_619K)) {
			if (pdic_data->is_client == CLIENT_OFF && pdic_data->is_host == HOST_OFF) {
				/* usb */
				pdic_data->is_client = CLIENT_ON;
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
				pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SNK;
#elif IS_ENABLED(CONFIG_TYPEC)
				pd_data->typec_power_role = TYPEC_SINK;
				typec_set_pwr_role(pd_data->port, pd_data->typec_power_role);
#endif
				pdic_event_work(pd_data, PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
						1/*attach*/, USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
			}
		}
#endif
	} else if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SOURCE) {
		ret = s2mf301_usbpd_check_abnormal_attach(pdic_data);
		if (ret == false) {
			dev_err(&i2c->dev, "%s, abnormal attach\n", __func__);
			ret = -1;
			goto out;
		}
		dev_info(dev, "SOURCE\n");
		ret = s2mf301_usbpd_check_accessory(pdic_data);
		if (ret < 0) {
			ret = -1;
			goto out;
		}
		pdic_data->detach_valid = false;
		pdic_data->power_role = PDIC_SOURCE;
		pdic_data->data_role = USBPD_DFP;
		s2mf301_dfp(i2c);
		s2mf301_src(i2c);
		usbpd_policy_reset(pd_data, PLUG_EVENT);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		dev_info(&i2c->dev, "%s %d: is_host = %d, is_client = %d\n", __func__,
					__LINE__, pdic_data->is_host, pdic_data->is_client);
		s2mf301_usbpd_check_client(pdic_data, CLIENT_ON);
		s2mf301_usbpd_check_host(pdic_data, HOST_OFF);
#else
		usbpd_manager_plug_attach(dev, ATTACHED_DEV_TYPE3_ADAPTER_MUIC);
#endif
		if (pdic_data->regulator_en) {
			ret = regulator_enable(pdic_data->regulator);
			if (ret)
				dev_err(&i2c->dev, "Failed to enable vconn LDO: %d\n", ret);
		}

		s2mf301_set_vconn_source(pd_data, USBPD_VCONN_ON);

//		msleep(tTypeCSinkWaitCap); /* dont over 310~620ms(tTypeCSinkWaitCap) */
		msleep(100); /* dont over 310~620ms(tTypeCSinkWaitCap) */
	} else {
		dev_err(dev, "%s, PLUG Error\n", __func__);
		ret = -1;
		goto out;
	}

	pdic_data->detach_valid = false;
	pdic_data->first_attach = true;

	s2mf301_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

out:
#if defined(CONFIG_S2MF301_PDIC_TRY_SNK)
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &val);
	val &= ~S2MF301_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, val);
#endif

	return ret;
}

static int s2mf301_check_init_port(struct s2mf301_usbpd_data *pdic_data)
{
	u8 data;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	ret = s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_MON2, &data);
	if (ret < 0)
		dev_err(dev, "%s, i2c read PLUG_MON2 error\n", __func__);

	if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SOURCE)
		return PDIC_SOURCE;
	else if ((data & S2MF301_PR_MASK) == S2MF301_PDIC_SINK)
		return PDIC_SINK;

	return -1;
}

static int s2mf301_usbpd_check_rid_detach(struct s2mf301_usbpd_data *pdic_data)
{
	u8 rid = 0;

	s2mf301_usbpd_read_reg(pdic_data->i2c, S2MF301_REG_ADC_STATUS, &rid);
	pdic_data->rid = (rid & S2MF301_PDIC_RID_MASK) >> S2MF301_PDIC_RID_SHIFT;
	if (pdic_data->rid == REG_RID_UNDF)
		pdic_data->rid = REG_RID_MAX;

	if ((pdic_data->rid != REG_RID_UNDF) && (pdic_data->rid != REG_RID_MAX) && (!pdic_data->first_attach))
		return true;
	else
		return false;
}

static irqreturn_t s2mf301_irq_thread(int irq, void *data)
{
	struct s2mf301_usbpd_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	int ret = 0;
	unsigned attach_status = 0, rid_status = 0;

#if IS_ENABLED(CONFIG_ARCH_QCOM)
	__pm_stay_awake(pdic_data->water_irq_wake);
#endif

	dev_info(dev, "%s\n", __func__);

	mutex_lock(&pdic_data->_mutex);

	s2mf301_poll_status(pd_data);

	if (s2mf301_get_status(pd_data, MSG_SOFTRESET)) {
		pdic_data->status_reg[0] = pdic_data->status_reg[1] = 0;
		usbpd_rx_soft_reset(pd_data);
	}

	if (s2mf301_get_status(pd_data, PLUG_DETACH)) {
		/* Enabled in !Factory, skip cc_state func for keep rid */
		ret = s2mf301_usbpd_check_rid_detach(pdic_data);
		if (ret)
			goto skip_detach;
		s2mf301_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP80);
		attach_status = s2mf301_get_status(pd_data, PLUG_ATTACH);
		rid_status = s2mf301_get_status(pd_data, MSG_RID);
		s2mf301_usbpd_detach_init(pdic_data);
		s2mf301_usbpd_notify_detach(pdic_data);
		if (attach_status) {
			ret = s2mf301_check_port_detect(pdic_data);
			if (ret >= 0) {
				if (rid_status) {
					s2mf301_usbpd_check_rid(pdic_data);
				}
				goto hard_reset;
			}
		}
		goto out;
	}

	if (s2mf301_get_status(pd_data, MSG_HARDRESET)) {
		mutex_lock(&pdic_data->pd_mutex);
		s2mf301_usbpd_set_pd_control(pdic_data, USBPD_CC_OFF);
		mutex_unlock(&pdic_data->pd_mutex);
		s2mf301_self_soft_reset(pdic_data);
		pdic_data->status_reg[0] = pdic_data->status_reg[1] = 0;
		if (pdic_data->power_role == PDIC_SOURCE)
			s2mf301_dfp(i2c);
		else
			s2mf301_ufp(i2c);
		usbpd_rx_hard_reset(dev);
		usbpd_kick_policy_work(dev);
		goto out;
	}

skip_detach:
	if (s2mf301_get_status(pd_data, PLUG_ATTACH) && !pdic_data->is_pr_swap) {
		if (s2mf301_check_port_detect(data) < 0)
			goto out;
	}

	if (s2mf301_get_status(pd_data, MSG_RID)) {
		s2mf301_usbpd_check_rid(pdic_data);
	}

	if (s2mf301_get_status(pd_data, MSG_NONE))
		goto out;
hard_reset:
	mutex_lock(&pdic_data->lpm_mutex);
	if (!pdic_data->lpm_mode)
		usbpd_kick_policy_work(dev);
	mutex_unlock(&pdic_data->lpm_mutex);
out:
	mutex_unlock(&pdic_data->_mutex);
#if IS_ENABLED(CONFIG_ARCH_QCOM)
		__pm_relax(pdic_data->water_irq_wake);
#endif

	return IRQ_HANDLED;
}

static void s2mf301_usbpd_probe_reset_work(struct work_struct *work)
{
	struct s2mf301_usbpd_data *pdic_data =
		container_of(work, struct s2mf301_usbpd_data, probe_reset_work.work);
	struct usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	struct policy_data *policy = &pd_data->policy;

	mutex_lock(&pdic_data->_mutex);
	s2mf301_info("%s, check detach(%d), pd_support(%d)\n", __func__,
			pdic_data->detach_valid, policy->pd_support);

	// attached but not pd negotiated -> force cc detach
	if (!pdic_data->detach_valid && !policy->pd_support) {
		s2mf301_usbpd_set_pd_control(pdic_data, USBPD_CC_OFF);
		s2mf301_assert_rp(pd_data);
		s2mf301_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP0);
		msleep(20);
		s2mf301_usbpd_detach_init(pdic_data);
		s2mf301_usbpd_notify_detach(pdic_data);
		s2mf301_info("%s, CC open ok\n", __func__);

		msleep(600);

		s2mf301_info("%s, CC DRP ok\n", __func__);
		s2mf301_assert_drp(pd_data);
		SET_STATUS(pdic_data->status_reg, PLUG_ATTACH);
		schedule_delayed_work(&pdic_data->plug_work, 0);
	}
	mutex_unlock(&pdic_data->_mutex);
}

static void s2mf301_usbpd_plug_work(struct work_struct *work)
{
	struct s2mf301_usbpd_data *pdic_data =
		container_of(work, struct s2mf301_usbpd_data, plug_work.work);

	s2mf301_irq_thread(-1, pdic_data);
}

#if IS_ENABLED(CONFIG_ARCH_QCOM)
static void s2mf301_usbpd_water_wake_work(struct work_struct *work)
{
	struct s2mf301_usbpd_data *pdic_data =
		container_of(work, struct s2mf301_usbpd_data, water_wake_work.work);

	__pm_relax(pdic_data->water_wake);
}
#endif

static int s2mf301_usbpd_reg_init(struct s2mf301_usbpd_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	u8 data = 0;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PHY_CTRL_IFG, &data);
	data |= S2MF301_PHY_IFG_35US << S2MF301_REG_IFG_SHIFT;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PHY_CTRL_IFG, data);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_MSG_SEND_CON, &data);
	data |= S2MF301_REG_MSG_SEND_CON_HARD_EN;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_MSG_SEND_CON, data);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PD_CTRL_2, &data);
	data &= ~S2MF301_REG_PD_OCP_MASK;
	data |= S2MF301_PD_OCP_575MV << S2MF301_REG_PD_OCP_SHIFT;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PD_CTRL_2, data);

	/* enable Rd monitor status when pd is attached at sink */
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_SET_MON, &data);
	data |= S2MF301_REG_PLUG_CTRL_SET_MON_RD;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_SET_MON, data);

	/* disable rd or vbus mux */
	/* Setting for PD Detection with VBUS */
	/* It is recognized that VBUS falls when PD line falls */
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, &data);
	data &= ~S2MF301_REG_RD_OR_VBUS_MUX_SEL;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_VBUS_MUX, data);

	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PHY_CTRL_00, 0x80);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_BMC_CTRL, &data);
	data |= 0x01 << 2;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_BMC_CTRL, data);

	/* set debounce time */
	/* 0F3C = 3900/300 = 13ms */
	s2mf301_usbpd_write_reg(i2c, 0x20, 0x3C);
	s2mf301_usbpd_write_reg(i2c, 0x21, 0x0F);

	/* set tCCDebounce = 100ms */
	s2mf301_usbpd_write_reg(i2c, 0x24, 0x1 << 4);
	s2mf301_usbpd_write_reg(i2c, 0x25, 0x0d);
	s2mf301_usbpd_write_reg(i2c, 0x24, 0x1 << 4 | 0x01 << 7);
	s2mf301_usbpd_write_reg(i2c, 0x24, 0);
	s2mf301_usbpd_write_reg(i2c, 0x25, 0);

	/* enable support acc */
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_PD_HOLD, &data);
	data |= 0x80;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_PD_HOLD, data);

	/*
	 * MSG_IRQ_SEL
	 * 1 : MSG_Type irq occurs with MSG_PASS
	 * 0 : MSG_Type irq occurs after Rx header
	 */
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PPS_CTRL, &data);
	data |= S2MF301_REG_PPS_CTRL_MSG_IRQ_SEL;
	data &= ~S2MF301_REG_PPS_ENABLE_MASK;
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PPS_CTRL, data);

	data = 0;
	data |= (S2MF301_REG_PLUG_CTRL_SSM_DISABLE |
			S2MF301_REG_PLUG_CTRL_VDM_DISABLE |
			S2MF301_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN);
	s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL, data);

	s2mf301_usbpd_read_reg(i2c,
		S2MF301_REG_PLUG_CTRL_PD12, &data);
	data &= ~S2MF301_REG_PLUG_CTRL_PD_MANUAL_MASK;
	s2mf301_usbpd_write_reg(i2c,
		S2MF301_REG_PLUG_CTRL_PD12, data);

	/* set Rd threshold to 400mV */
	s2mf301_usbpd_write_reg(i2c,
		S2MF301_REG_PLUG_CTRL_SET_RD_2,
		S2MF301_THRESHOLD_600MV);
	s2mf301_usbpd_write_reg(i2c,
		S2MF301_REG_PLUG_CTRL_SET_RP_2,
		S2MF301_THRESHOLD_1200MV);
#ifdef CONFIG_SEC_FACTORY
	s2mf301_usbpd_write_reg(i2c,
		S2MF301_REG_PLUG_CTRL_SET_RD,
		S2MF301_THRESHOLD_342MV | 0x40);
#else
	s2mf301_usbpd_write_reg(i2c,
		S2MF301_REG_PLUG_CTRL_SET_RD,
		S2MF301_THRESHOLD_257MV | 0x40);
#endif
	s2mf301_usbpd_write_reg(i2c,
		S2MF301_REG_PLUG_CTRL_SET_RP,
		S2MF301_THRESHOLD_MAX);

	if (_data->vconn_en) {
		/* Off Manual Rd setup & On Manual Vconn setup */
		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, &data);
		data &= ~(S2MF301_REG_PLUG_CTRL_RpRd_MANUAL_EN_MASK);
		data |= S2MF301_REG_PLUG_CTRL_VCONN_MANUAL_EN;
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_PLUG_CTRL_RpRd, data);
	}
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_PM_S2MF301)
	s2mf301_usbpd_set_pmeter_mode(_data, S2MF301_PM_TYPE_VCHGIN);
#endif
	s2mf301_usbpd_set_vconn_manual(_data, true);

	return 0;
}

static irqreturn_t s2mf301_irq_isr(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

static int s2mf301_usbpd_irq_init(struct s2mf301_usbpd_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;
	int ret = 0;

	if (!_data->irq_gpio) {
		dev_err(dev, "%s No interrupt specified\n", __func__);
		return -ENXIO;
	}

	ret = gpio_request(_data->irq_gpio, "usbpd_irq");
	if (ret) {
		s2mf301_err("%s: failed requesting gpio %d\n",
			__func__, _data->irq_gpio);
		return ret;
	}
	gpio_direction_input(_data->irq_gpio);
	i2c->irq = gpio_to_irq(_data->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, s2mf301_irq_isr,
				s2mf301_irq_thread,
#if IS_ENABLED(CONFIG_ARCH_QCOM)
				(IRQF_TRIGGER_LOW | IRQF_ONESHOT),
#else
				(IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_NO_SUSPEND),
#endif
				"s2mf301-usbpd", _data);
		if (ret < 0) {
			dev_err(dev, "%s failed to request irq(%d)\n",
					__func__, i2c->irq);
			gpio_free(_data->irq_gpio);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			dev_err(dev, "%s failed to enable wakeup src\n",
					__func__);
	}

	if (_data->lpm_mode)
		s2mf301_set_irq_enable(_data, 0, 0, 0, 0, 0, 0);
	else
		s2mf301_set_irq_enable(_data,
			ENABLED_INT_0, ENABLED_INT_1,
			ENABLED_INT_2, ENABLED_INT_3,
			ENABLED_INT_4, ENABLED_INT_5);

	return ret;
}

static void s2mf301_usbpd_reset_osc_clk(struct s2mf301_usbpd_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	u8 saved_data = 0, otp_0a = 0;

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_DUMMY, &saved_data);
	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, &otp_0a);
	s2mf301_info("%s, OTP(0x%x), saved(0x%x)\n", __func__, otp_0a, saved_data);

	if (saved_data & (1 << 7)) {
		/* saved data ok */
		if ((otp_0a & 0x7F) != (saved_data & 0x7F)) {
			s2mf301_info("%s, saved, but not same!\n", __func__);

			otp_0a &= ~(0x7F);
			otp_0a |= (saved_data & 0x7F);
    			s2mf301_usbpd_write_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, otp_0a);
			s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ANALOG_OTP_0A, &otp_0a);
			s2mf301_info("%s, recover otp register new otp(0x%x)\n", __func__, otp_0a);
		} else {
			s2mf301_info("%s, saved and same(do nothing)!\n", __func__);
		}
	} else {
		/* no saved data */
		saved_data = (1 << 7) | ( otp_0a & 0x7F);
		s2mf301_usbpd_write_reg(i2c, S2MF301_REG_DUMMY, saved_data);

		s2mf301_usbpd_read_reg(i2c, S2MF301_REG_DUMMY, &saved_data);
		s2mf301_info("%s, new saved(0x%x)\n", __func__, saved_data);
	}
}

static void s2mf301_usbpd_init_configure(struct s2mf301_usbpd_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = _data->dev;
	u8 rid = 0;
	int pdic_port = 0;
	struct power_supply *psy_muic = NULL;
	union power_supply_propval val;
	int ret = 0;

	INIT_DELAYED_WORK(&_data->probe_reset_work,
		s2mf301_usbpd_probe_reset_work);

	s2mf301_usbpd_reset_osc_clk(_data);

	s2mf301_usbpd_read_reg(i2c, S2MF301_REG_ADC_STATUS, &rid);
	s2mf301_info("%s, rid reg(0x%x)\n", __func__, rid);

	rid = (rid & S2MF301_PDIC_RID_MASK) >> S2MF301_PDIC_RID_SHIFT;

	_data->rid = rid;

	_data->detach_valid = false;

	/* if there is rid, assume that booted by normal mode */
	if (rid) {
		_data->lpm_mode = false;
		_data->is_factory_mode = false;
		s2mf301_usbpd_set_rp_scr_sel(_data, PLUG_CTRL_RP80);
#if IS_ENABLED(CONFIG_SEC_FACTORY)
#if 0	/* TBD */
		if (factory_mode) {
			if (rid != REG_RID_523K) {
				dev_err(dev, "%s : In factory mode, but RID is not 523K\n", __func__);
			} else {
				dev_err(dev, "%s : In factory mode, but RID is 523K OK\n", __func__);
				_data->is_factory_mode = true;
			}
		}
#else
		if (rid != REG_RID_523K)
			dev_err(dev, "%s : factory mode, but RID : 523K\n",
				__func__);
		else
			_data->is_factory_mode = true;
#endif
#endif
		s2mf301_usbpd_set_pd_control(_data, USBPD_CC_ON);
	} else {
		s2mf301_usbpd_self_reset(_data);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		psy_muic = get_power_supply_by_name("muic-manager");
#endif
		if (psy_muic) {
            ret = psy_muic->desc->get_property(psy_muic, (enum power_supply_property)POWER_SUPPLY_LSI_PROP_PM_VCHGIN, &val);
            if (ret < 0)
                val.intval = 1;
        } else
            val.intval = 1;


		s2mf301_usbpd_test_read(_data);
		s2mf301_usbpd_set_vbus_wakeup(_data, VBUS_WAKEUP_DISABLE);
		s2mf301_usbpd_set_vbus_wakeup(_data, VBUS_WAKEUP_ENABLE);
		usleep_range(1000, 1100);
		pdic_port = s2mf301_check_init_port(_data);
		dev_err(dev, "%s : Initial abnormal state to LPM Mode, vbus(%d), pdic_port(%d)\n",
								__func__, val.intval, pdic_port);
		s2mf301_set_normal_mode(_data);
		if (val.intval || pdic_port >= PDIC_SINK) {
			msleep(200);
			_data->detach_valid = true;
			if (_data->is_water_detect && is_lpcharge_pdic_param())
				s2mf301_info("%s, water detected in lpcharge!! skip initial lpm\n", __func__);
			else
			s2mf301_usbpd_init_tx_hard_reset(_data);
			_data->detach_valid = false;
			s2mf301_usbpd_set_pd_control(_data, USBPD_CC_OFF);
			_data->lpm_mode = true;
			msleep(150); /* for abnormal PD TA */
			_data->is_factory_mode = false;
#if	(!IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_PDIC_MODE_BY_MUIC))
			if (pdic_port == PDIC_SOURCE)
				s2mf301_set_normal_mode(_data);
#else
			s2mf301_set_normal_mode(_data);
			_data->lpm_mode = false;
#endif
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
			/* check pd negotiated after 1 sec */
			schedule_delayed_work(&_data->probe_reset_work,
					msecs_to_jiffies(1000));
#endif
		} else
			s2mf301_usbpd_set_pd_control(_data, USBPD_CC_OFF);
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
		if (_data->is_water_detect) {
			_data->water_status = S2M_WATER_STATUS_INVALID;
			s2mf301_usbpd_water_set_status(_data, S2M_WATER_STATUS_WATER);
		}
#endif
	}
}

static void s2mf301_usbpd_pdic_data_init(struct s2mf301_usbpd_data *_data)
{
	u8 rid = 0;

	s2mf301_usbpd_read_reg(_data->i2c, S2MF301_REG_ADC_STATUS, &rid);
	_data->rid = (rid & S2MF301_PDIC_RID_MASK) >> S2MF301_PDIC_RID_SHIFT;
	if (_data->rid == REG_RID_UNDF)
		_data->rid = REG_RID_MAX;
	s2mf301_info("%s, rid(0x%x)\n", __func__, _data->rid);

	_data->vconn_source = USBPD_VCONN_OFF;
	_data->is_host = 0;
	_data->is_client = 0;
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
	_data->data_role_dual = 0;
	_data->power_role_dual = 0;
#elif IS_ENABLED(CONFIG_TYPEC)
	//_data->typec_power_role = TYPEC_SINK;
	//_data->typec_data_role = TYPEC_DEVICE;
#endif
	_data->detach_valid = true;
	_data->is_otg_vboost = false;
	_data->is_otg_reboost = false;
	_data->is_pr_swap = false;
	_data->rp_lvl = PLUG_CTRL_RP80;
	_data->vbus_short = false;
	_data->vbus_short_check = false;
	_data->pd_vbus_short_check = false;
	_data->vbus_short_check_cnt = 0;
	_data->pm_cc1 = 0;
	_data->pm_cc2 = 0;
	_data->is_killer = 0;
	_data->first_attach = 0;
	_data->first_goodcrc = 0;
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	_data->is_water_detect = false;
	_data->water_gpadc_short = 2500;
#endif

	_data->clk_offset = 23;
	_data->clk_offset2 = 21;
	_data->chip_rev = -1;
}

static int of_s2mf301_dt(struct device *dev,
			struct s2mf301_usbpd_data *_data)
{
	struct device_node *np_usbpd = dev->of_node;
	int ret = 0;

	if (np_usbpd == NULL) {
		dev_err(dev, "%s np NULL\n", __func__);
		return -EINVAL;
	}

	_data->irq_gpio = of_get_named_gpio(np_usbpd,
						"usbpd,usbpd_int", 0);
	if (_data->irq_gpio < 0) {
		dev_err(dev, "error reading usbpd irq = %d\n",
					_data->irq_gpio);
		_data->irq_gpio = 0;
	}

	_data->vbus_dischg_gpio = of_get_named_gpio(np_usbpd,
						"usbpd,vbus_discharging", 0);
	if (gpio_is_valid(_data->vbus_dischg_gpio))
		s2mf301_info("%s vbus_discharging = %d\n",
					__func__, _data->vbus_dischg_gpio);

	if (of_find_property(np_usbpd, "vconn-en", NULL))
		_data->vconn_en = true;
	else
		_data->vconn_en = false;

	if (of_find_property(np_usbpd, "regulator-en", NULL))
		_data->regulator_en = true;
	else
		_data->regulator_en = false;

	return ret;
}

static int s2mf301_usbpd_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct s2mf301_usbpd_data *pdic_data;
	struct usbpd_data *pd_data;
	struct device *dev = &i2c->dev;
	int ret = 0;
	union power_supply_propval val;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	pdic_data = kzalloc(sizeof(struct s2mf301_usbpd_data), GFP_KERNEL);
	if (!pdic_data) {
		dev_err(dev, "%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	pdic_data->dev = &i2c->dev;
	pdic_data->i2c = i2c;
	i2c_set_clientdata(i2c, pdic_data);

	ret = of_s2mf301_dt(&i2c->dev, pdic_data);
	if (ret < 0)
		dev_err(dev, "%s: not found dt!\n", __func__);

	mutex_init(&pdic_data->_mutex);
	mutex_init(&pdic_data->lpm_mutex);
	mutex_init(&pdic_data->pd_mutex);
	mutex_init(&pdic_data->water_mutex);
	mutex_init(&pdic_data->otg_mutex);
	mutex_init(&pdic_data->s2m_water_mutex);

#if defined(CONFIG_S2MF301_PDIC_TRY_SNK)
	alarm_init(&pdic_data->srcdet_alarm, ALARM_BOOTTIME, s2mf301_usbpd_try_snk_alarm_srcdet);
	alarm_init(&pdic_data->snkdet_alarm, ALARM_BOOTTIME, s2mf301_usbpd_try_snk_alarm_snkdet);
#endif

#if IS_ENABLED(CONFIG_MUIC_SUPPORT_POWERMETER)
	pdic_data->psy_pm = get_power_supply_by_name("s2mf301-pmeter");
#endif
	if (!pdic_data->psy_pm)
		s2mf301_err("%s: Fail to get pmeter\n", __func__);

	s2mf301_usbpd_reg_init(pdic_data);

#if IS_BUILTIN(CONFIG_PDIC_NOTIFIER)
	pdic_notifier_init();
#endif

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	ret = s2mf301_usbpd_get_pmeter_volt(pdic_data);
	s2mf301_info("%s, Vbus (%d)mV\n", __func__, pdic_data->pm_chgin);

	if (!ret && pdic_data->pm_chgin >= 4000)
		s2mf301_power_off_water_check(pdic_data);

	s2mf301_water_init(&pdic_data->water);
#endif

	s2mf301_usbpd_init_configure(pdic_data);
	s2mf301_usbpd_pdic_data_init(pdic_data);

	if (pdic_data->regulator_en) {
		pdic_data->regulator = devm_regulator_get(dev, "vconn");
		if (IS_ERR(pdic_data->regulator)) {
			dev_err(dev, "%s: not found regulator vconn\n", __func__);
			pdic_data->regulator_en = false;
		} else
			ret = regulator_disable(pdic_data->regulator);
	}

	ret = usbpd_init(dev, pdic_data);
	if (ret < 0) {
		dev_err(dev, "failed on usbpd_init\n");
		goto err_return;
	}

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	if (pdic_data->power_off_water_detected)
		s2mf301_power_off_water_notify(pdic_data);
	else
		s2mf301_water_init_work_start(&pdic_data->water);
#endif

	pd_data = dev_get_drvdata(dev);
	pd_data->ip_num = S2MF301_USBPD_IP;
	pd_data->pmeter_name = "s2mf301-pmeter";

	usbpd_set_ops(dev, &s2mf301_ops);

	pdic_data->pdic_queue =
	    alloc_workqueue("%s", WQ_MEM_RECLAIM, 1, dev_name(dev));
	if (!pdic_data->pdic_queue) {
		dev_err(dev,
			"%s: Fail to Create Workqueue\n", __func__);
		goto err_return;
	}

#if IS_ENABLED(CONFIG_TYPEC)
	ret = typec_init(pd_data);
	if (ret < 0) {
		s2mf301_err("failed to init typec\n");
		goto err_return;
	}
	pdic_data->rprd_mode_change = s2mf301_rprd_mode_change;
#endif

	INIT_DELAYED_WORK(&pdic_data->plug_work,
		s2mf301_usbpd_plug_work);
	INIT_DELAYED_WORK(&pdic_data->vbus_dischg_off_work,
			s2mf301_vbus_dischg_off_work);
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	INIT_DELAYED_WORK(&pdic_data->check_facwater,
		s2mf301_usbpd_check_facwater);
#endif

#if IS_ENABLED(CONFIG_ARCH_QCOM)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 188)
	wakeup_source_init(pdic_data->water_wake, "water_wake");   // 4.19 R
	if (!(pdic_data->water_wake)) {
		pdic_data->water_wake = wakeup_source_create("water_wake"); // 4.19 Q
		if (pdic_data->water_wake)
			wakeup_source_add(pdic_data->water_wake);
	}
	wakeup_source_init(pdic_data->water_irq_wake, "water_irq_wake");   // 4.19 R
	if (!(pdic_data->water_irq_wake)) {
		pdic_data->water_irq_wake = wakeup_source_create("water_irq_wake"); // 4.19 Q
		if (pdic_data->water_irq_wake)
			wakeup_source_add(pdic_data->water_irq_wake);
	}
#else
	pdic_data->water_wake = wakeup_source_register(NULL, "water_wake"); // 5.4 R
	pdic_data->water_irq_wake = wakeup_source_register(NULL, "water_irq_wake"); // 5.4 R
#endif
	INIT_DELAYED_WORK(&pdic_data->water_wake_work,
		s2mf301_usbpd_water_wake_work);
#endif

	ret = s2mf301_usbpd_irq_init(pdic_data);
	if (ret) {
		dev_err(dev, "%s: failed to init irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	ret = usbpd_manager_psy_init(pd_data, &i2c->dev);
	if (ret < 0)
		s2mf301_err("faled to register the pdic psy.\n");

	device_init_wakeup(dev, 1);

	if (pdic_data->detach_valid) {
		mutex_lock(&pdic_data->_mutex);
		s2mf301_check_port_detect(pdic_data);
		s2mf301_usbpd_check_rid(pdic_data);
		mutex_unlock(&pdic_data->_mutex);
	}

	s2mf301_irq_thread(-1, pdic_data);

	if (pdic_data->rid != REG_RID_UNDF && pdic_data->rid != REG_RID_MAX)
		s2mf301_notify_pdic_rid(pdic_data, pdic_data->rid);

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_pdic_notifier_register(&pdic_data->type3_nb,
	       type3_handle_notification,
	       MUIC_NOTIFY_DEV_PDIC);
#endif
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
	ret = dual_role_init(pdic_data);
	if (ret < 0) {
		s2mf301_err("unable to allocate dual role descriptor\n");
		goto fail_init_irq;
	}
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdic_data->psy_muic = get_power_supply_by_name("muic-manager");
#endif
	if (!pdic_data->psy_muic)
		s2mf301_err("%s: Fail to get psy_muic\n", __func__);
	else {
		val.intval = 1;
		pdic_data->psy_muic->desc->set_property(pdic_data->psy_muic,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_PD_SUPPORT, &val);
	}


	dev_info(dev, "%s s2mf301 usbpd driver uploaded!\n", __func__);

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, pdic_data);
err_return:
	return ret;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mf301_usbpd_suspend(struct device *dev)
{
	struct usbpd_data *_data = dev_get_drvdata(dev);
	struct s2mf301_usbpd_data *pdic_data = _data->phy_driver_data;

	if (device_may_wakeup(dev))
		enable_irq_wake(pdic_data->i2c->irq);

#if defined(CONFIG_ARCH_EXYNOS)
	disable_irq(pdic_data->i2c->irq);
#endif
	return 0;
}

static int s2mf301_usbpd_resume(struct device *dev)
{
	struct usbpd_data *_data = dev_get_drvdata(dev);
	struct s2mf301_usbpd_data *pdic_data = _data->phy_driver_data;

	if (device_may_wakeup(dev))
		disable_irq_wake(pdic_data->i2c->irq);

#if defined(CONFIG_ARCH_EXYNOS)
	enable_irq(pdic_data->i2c->irq);
#endif
	return 0;
}
#else
#define s2mf301_muic_suspend NULL
#define s2mf301_muic_resume NULL
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void s2mf301_usbpd_remove(struct i2c_client *i2c)
#else
static int s2mf301_usbpd_remove(struct i2c_client *i2c)
#endif
{
	struct s2mf301_usbpd_data *_data = i2c_get_clientdata(i2c);
	struct usbpd_data *pd_data = dev_get_drvdata(&i2c->dev);

	if (_data) {
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
		devm_dual_role_instance_unregister(_data->dev,
						_data->dual_role);
		devm_kfree(_data->dev, _data->desc);
#elif IS_ENABLED(CONFIG_TYPEC)
		typec_unregister_port(pd_data->port);
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		pdic_register_switch_device(0);
		if (pd_data->ppdic_data && pd_data->ppdic_data->misc_dev)
			pdic_misc_exit();
#endif
		disable_irq_wake(_data->i2c->irq);
		free_irq(_data->i2c->irq, _data);
		gpio_free(_data->irq_gpio);
		s2mf301_usbpd_set_vbus_dischg_gpio(_data, 0);
		mutex_destroy(&_data->_mutex);
		mutex_destroy(&_data->water_mutex);
		i2c_set_clientdata(_data->i2c, NULL);
#if IS_ENABLED(CONFIG_ARCH_QCOM)
		wakeup_source_unregister(_data->water_wake);
#endif
		kfree(_data);
	}
	if (pd_data) {
		wakeup_source_unregister(pd_data->policy_wake);
		mutex_destroy(&pd_data->accept_mutex);
		mutex_destroy(&pd_data->softreset_mutex);
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static const struct i2c_device_id s2mf301_usbpd_i2c_id[] = {
	{ USBPD_S2MF301_NAME, 1 },
	{}
};
MODULE_DEVICE_TABLE(i2c, s2mf301_usbpd_i2c_id);

static struct of_device_id s2mf301_usbpd_i2c_dt_ids[] = {
	{ .compatible = "s2mf301-usbpd" },
	{}
};

static void s2mf301_usbpd_shutdown(struct i2c_client *i2c)
{
	struct s2mf301_usbpd_data *_data = i2c_get_clientdata(i2c);

	if (!_data->i2c)
		return;
}

static usbpd_phy_ops_type s2mf301_ops = {
	.tx_msg			= s2mf301_tx_msg,
	.rx_msg			= s2mf301_rx_msg,
	.hard_reset		= s2mf301_hard_reset,
	.soft_reset		= s2mf301_soft_reset,
	.set_power_role		= s2mf301_set_power_role,
	.get_power_role		= s2mf301_get_power_role,
	.set_data_role		= s2mf301_set_data_role,
	.get_data_role		= s2mf301_get_data_role,
	.set_vconn_source	= s2mf301_set_vconn_source,
	.get_vconn_source	= s2mf301_get_vconn_source,
	.get_status			= s2mf301_get_status,
	.poll_status		= s2mf301_poll_status,
	.driver_reset		= s2mf301_driver_reset,
	.set_otg_control	= s2mf301_set_otg_control,
	.get_vbus_short_check	= s2mf301_get_vbus_short_check,
	.pd_vbus_short_check	= s2mf301_pd_vbus_short_check,
	.set_pd_control		= s2mf301_set_pd_control,
	.set_chg_lv_mode	= s2mf301_set_chg_lv_mode,
#if IS_ENABLED(CONFIG_CHECK_CTYPE_SIDE) || IS_ENABLED(CONFIG_PDIC_SYSFS)
	.get_side_check		= s2mf301_get_side_check,
#endif
	.pr_swap			= s2mf301_pr_swap,
	.vbus_on_check		= s2mf301_vbus_on_check,
	.set_rp_control		= s2mf301_set_rp_control,
	.pd_instead_of_vbus = s2mf301_pd_instead_of_vbus,
	.op_mode_clear		= s2mf301_op_mode_clear,
#if IS_ENABLED(CONFIG_TYPEC)
	.set_pwr_opmode		= s2mf301_set_pwr_opmode,
#endif
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	.ops_power_off_water	= s2mf301_usbpd_ops_power_off_water,
	.ops_get_is_water_detect	= s2mf301_usbpd_ops_get_is_water_detect,
	.ops_prt_water_threshold	= s2mf301_usbpd_ops_prt_water_threshold,
	.ops_set_water_threshold	= s2mf301_usbpd_ops_set_water_threshold,
#endif
	.energy_now			= s2mf301_usbpd_energy_now,
	.authentic				= s2mf301_usbpd_authentic,
	.set_usbpd_reset		= s2mf301_usbpd_set_usbpd_reset,
	.vbus_onoff			= s2mf301_usbpd_vbus_onoff,
	.get_detach_valid		= s2mf301_usbpd_get_detach_valid,
	.rprd_mode_change		= s2mf301_rprd_mode_change,
	.irq_control			= s2mf301_usbpd_irq_control,
	.set_is_otg_vboost		= s2mf301_usbpd_set_is_otg_vboost,
	.ops_get_lpm_mode		= s2mf301_usbpd_ops_get_lpm_mode,
	.ops_get_rid			= s2mf301_usbpd_ops_get_rid,
	.ops_sysfs_lpm_mode		= s2mf301_usbpd_ops_sysfs_lpm_mode,
	.ops_control_option_command	= s2mf301_usbpd_ops_control_option_command,
#if IS_ENABLED(CONFIG_HICCUP_CC_DISABLE)
	.ops_ccopen_req			= s2mf301_usbpd_ops_ccopen_req,
#endif
	.set_pcp_clk			= s2mf301_usbpd_set_pcp_clk,
#if IS_ENABLED(CONFIG_S2MF301_PDIC_SUPPORT_S2MC501)
	.pps_enable				= s2mf301_pps_enable,
	.get_pps_enable			= s2m301_get_pps_enable,
#endif
	.set_revision		= s2mf301_set_revision,
	.get_rp_level		= s2mf301_get_rp_level,
	.ops_set_clk_offset	= s2mf301_usbpd_set_clk_offset,
	.ops_check_pps_irq_reduce_clk	= s2mf301_usbpd_check_pps_irq_reduce_clk,
	.ops_check_pps_irq_tx_req	= s2mf301_usbpd_check_pps_irq_tx_req,
	.ops_check_pps_irq		= s2mf301_usbpd_check_pps_irq,
#if IS_ENABLED(CONFIG_S2M_PDIC_MANUAL_RETRY)
	.ops_manual_retry	= s2mf301_ops_set_manual_retry,
#endif
};

#if IS_ENABLED(CONFIG_PM)
const struct dev_pm_ops s2mf301_usbpd_pm = {
	.suspend = s2mf301_usbpd_suspend,
	.resume = s2mf301_usbpd_resume,
};
#endif

static struct i2c_driver s2mf301_usbpd_driver = {
	.driver		= {
		.name	= USBPD_S2MF301_NAME,
		.of_match_table	= s2mf301_usbpd_i2c_dt_ids,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mf301_usbpd_pm,
#endif /* CONFIG_PM */
	},
	.probe		= s2mf301_usbpd_probe,
	.remove		= s2mf301_usbpd_remove,
	.shutdown	= s2mf301_usbpd_shutdown,
	.id_table	= s2mf301_usbpd_i2c_id,
};

static int __init s2mf301_usbpd_init(void)
{
	s2mf301_err("%s\n", __func__);
	return i2c_add_driver(&s2mf301_usbpd_driver);
}
late_initcall(s2mf301_usbpd_init);

static void __exit s2mf301_usbpd_exit(void)
{
	i2c_del_driver(&s2mf301_usbpd_driver);
}
module_exit(s2mf301_usbpd_exit);

MODULE_DESCRIPTION("S2MF301 USB PD driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: muic_s2mf301");
