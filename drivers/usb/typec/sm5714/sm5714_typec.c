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
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <linux/sec_batt.h>
#endif
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#else
#include <linux/battery/sec_pd.h>
#endif
#include <linux/muic/common/muic.h>
#include <linux/usb/typec/sm/sm5714/sm5714_pd.h>
#include <linux/usb/typec/sm/sm5714/sm5714_typec.h>
#include <linux/mfd/sm5714-private.h>
#include <linux/usb/typec/common/sm/pdic_sysfs.h>
#include <linux/usb/typec/common/sm/pdic_param.h>
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER_SM5714)
#include <linux/vbus_notifier.h>
#endif /* CONFIG_VBUS_NOTIFIER */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "../../../battery/common/include/sec_charging_common.h"
#endif
#include <linux/usb_notify.h>


#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
extern int factory_mode;
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static enum pdic_sysfs_property sm5714_sysfs_properties[] = {
	PDIC_SYSFS_PROP_CHIP_NAME,
	PDIC_SYSFS_PROP_LPM_MODE,
	PDIC_SYSFS_PROP_STATE,
	PDIC_SYSFS_PROP_RID,
	PDIC_SYSFS_PROP_CTRL_OPTION,
	PDIC_SYSFS_PROP_FW_WATER,
	PDIC_SYSFS_PROP_ACC_DEVICE_VERSION,
	PDIC_SYSFS_PROP_CONTROL_GPIO,
	PDIC_SYSFS_PROP_USBPD_IDS,
	PDIC_SYSFS_PROP_USBPD_TYPE,
#if defined(CONFIG_SEC_FACTORY)
	PDIC_SYSFS_PROP_CC_PIN_STATUS,
	PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE,
        PDIC_SYSFS_PROP_VBUS_ADC,
#endif
};
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
static enum dual_role_property sm5714_fusb_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};
#endif

struct i2c_client *test_i2c;
static usbpd_phy_ops_type sm5714_ops;

static int sm5714_usbpd_reg_init(struct sm5714_phydrv_data *_data);
static void sm5714_driver_reset(void *_data);
static void sm5714_get_short_state(void *_data, bool *val);
void sm5714_set_enable_pd_function(void *_data, int enable);

static int sm5714_usbpd_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	ret &= 0xff;
	*dest = ret;
	return 0;
}

static int sm5714_usbpd_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	return ret;
}

static int sm5714_usbpd_multi_read(struct i2c_client *i2c,
		u8 reg, int count, u8 *buf)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	return 0;
}

static int sm5714_usbpd_multi_write(struct i2c_client *i2c,
		u8 reg, int count, u8 *buf)
{
	int ret;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	return 0;
}

static void sm5714_set_dfp(struct i2c_client *i2c)
{
	u8 data;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL2, &data);
	data |= 0x01;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, data);
}

static void sm5714_set_ufp(struct i2c_client *i2c)
{
	u8 data;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL2, &data);
	data &= ~0x01;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, data);
}

static void sm5714_set_src(struct i2c_client *i2c)
{
	u8 data;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL2, &data);
	data |= 0x02;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, data);
}

static void sm5714_set_snk(struct i2c_client *i2c)
{
	u8 data;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL2, &data);
	data &= ~0x02;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, data);
}

static int sm5714_set_attach(struct sm5714_phydrv_data *pdic_data, u8 mode)
{
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	if (mode == TYPE_C_ATTACH_DFP) {
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL1, 0x49);
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, 0x81);
	} else if (mode == TYPE_C_ATTACH_UFP) {
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL1, 0x45);
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, 0x82);
	}

	dev_info(dev, "%s sm5714 force to attach\n", __func__);

	return ret;
}

static int sm5714_set_detach(struct sm5714_phydrv_data *pdic_data, u8 mode)
{
	u8 data;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL3, &data);
	data |= 0x08;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, data);

	dev_info(dev, "%s sm5714 force to detach\n", __func__);

	return ret;
}

static int sm5714_set_vconn_source(void *_data, int val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 reg_data = 0, reg_val = 0, attach_type = 0;
	int cable_type = 0;

	if (!pdic_data->vconn_en) {
		pr_err("%s, not support vconn source\n", __func__);
		return -1;
	}

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_STATUS, &reg_val);

	attach_type = (reg_val & SM5714_ATTACH_TYPE);
	cable_type = (reg_val >> SM5714_CABLE_TYPE_SHIFT) ?
			PWR_CABLE : NON_PWR_CABLE;

	dev_info(dev, "%s ON=%d REG=0x%x, ATTACH_TYPE = %d, CABLE_TYPE = %d\n",
			__func__, val, reg_val, attach_type, cable_type);

	if (val == USBPD_VCONN_ON) {
		if (cable_type || pdic_data->pd_support) {
			reg_data = (reg_val & 0x20) ? 0x1A : 0x19;
			sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL5, reg_data);
		}
	} else if (val == USBPD_VCONN_OFF) {
		reg_data = 0x18;
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL5, reg_data);
	} else
		return -1;

	pdic_data->vconn_source = val;
	return 0;
}

static int sm5714_set_normal_mode(struct sm5714_phydrv_data *pdic_data)
{
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL4, 0x00);

	pdic_data->lpm_mode = false;

	dev_info(dev, "%s sm5714 exit lpm mode\n", __func__);

	return ret;
}

static int sm5714_set_lpm_mode(struct sm5714_phydrv_data *pdic_data)
{
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	pdic_data->lpm_mode = true;

#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL4, 0x03);
#else
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL4, 0x00);
#endif

	dev_info(dev, "%s sm5714 enter lpm mode\n", __func__);

	return ret;
}

static void sm5714_adc_value_read(void *_data, u8 *adc_value)
{
	struct sm5714_phydrv_data *pdic_data = _data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 adc_done = 0, reg_data = 0;
	int retry = 0;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_ADC_CNTL1, &adc_done);
	for (retry = 0; retry < 5; retry++) {
		if (!(adc_done & SM5714_ADC_DONE)) {
			pr_info("%s, ADC_DONE is not yet, retry : %d\n", __func__, retry+1);
			sm5714_usbpd_read_reg(i2c, SM5714_REG_ADC_CNTL1, &adc_done);
		} else {
			sm5714_usbpd_read_reg(i2c, SM5714_REG_ADC_CNTL2, &reg_data);
			break;
		}
	}
	*adc_value = reg_data;
}
#if defined(CONFIG_SEC_FACTORY)
static int sm5714_vbus_adc_read(void *_data)
{
        struct sm5714_phydrv_data *pdic_data = _data;
        struct i2c_client *i2c = NULL;
        u8 vbus_adc = 0, status1 = 0;

        if (!pdic_data)
                return -ENXIO;

        i2c = pdic_data->i2c;
        if (!i2c)
                return -ENXIO;

        sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS1, &status1);
        sm5714_usbpd_write_reg(i2c, SM5714_REG_ADC_CNTL1, SM5714_ADC_PATH_SEL_VBUS);
        sm5714_adc_value_read(pdic_data, &vbus_adc);

        pr_info("%s, STATUS1 = 0x%x, VBUS_VOLT : 0x%x\n",
                        __func__, status1, vbus_adc);

        return vbus_adc; /* 0 is OK, others are NG */
}
#endif
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
static void sm5714_process_cc_water_det(void *data, int state)
{
	struct sm5714_phydrv_data *pdic_data = data;

	sm5714_pdic_event_work(pdic_data,
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
			PDIC_NOTIFY_DEV_BATTERY,
#else
			PDIC_NOTIFY_DEV_BATT,
#endif
			PDIC_NOTIFY_ID_WATER, state/*attach*/,
			USB_STATUS_NOTIFY_DETACH, 0);
	pr_info("%s, water state : %d\n", __func__, state);
}
#endif

static void sm5714_corr_sbu_volt_read(void *_data, u8 *adc_sbu1,
				u8 *adc_sbu2, int mode)
{
	struct sm5714_phydrv_data *pdic_data = _data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 adc_value1 = 0, adc_value2 = 0;

	if (mode) {
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL5, 0x98);
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL6, 0x40);
		usleep_range(5000, 5100);
	}
	sm5714_usbpd_write_reg(i2c, SM5714_REG_ADC_CNTL1, SM5714_ADC_PATH_SEL_SBU1);
	sm5714_adc_value_read(pdic_data, &adc_value1);
	*adc_sbu1 = adc_value1;

	sm5714_usbpd_write_reg(i2c, SM5714_REG_ADC_CNTL1, SM5714_ADC_PATH_SEL_SBU2);
	sm5714_adc_value_read(pdic_data, &adc_value2);
	*adc_sbu2 = adc_value2;

	if (mode) {
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL5, 0x00);
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL6, 0x00);
	}

	pr_info("%s, mode : %d, SBU1_VOLT : 0x%x, SBU2_VOLT : 0x%x\n",
			__func__, mode, adc_value1, adc_value2);
}

#if defined(CONFIG_SM5714_SUPPORT_SBU)
void sm5714_short_state_check(void *_data)
{
	struct sm5714_phydrv_data *pdic_data = _data;
	u8 adc_sbu1, adc_sbu2, adc_sbu3, adc_sbu4;
#if IS_ENABLED(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
	int event = 0;
#endif

	if (pdic_data->is_cc_abnormal_state || pdic_data->is_otg_vboost)
		return;

	sm5714_corr_sbu_volt_read(pdic_data, &adc_sbu1, &adc_sbu2,
			SBU_SOURCING_OFF);
	if (adc_sbu1 > 0x2C || adc_sbu2 > 0x2C) {
		sm5714_corr_sbu_volt_read(pdic_data, &adc_sbu3,
				&adc_sbu4, SBU_SOURCING_ON);
		if ((adc_sbu1 < 0x4 || adc_sbu2 < 0x4) &&
				(adc_sbu3 > 0x2C || adc_sbu4 > 0x2C)) {
			pdic_data->is_sbu_abnormal_state = true;
			pr_info("%s, SBU-VBUS SHORT\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
			if (o_notify)
				inc_hw_param(o_notify,
						USB_CCIC_VBUS_SBU_SHORT_COUNT);
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
			event = NOTIFY_EXTRA_SYSMSG_SBU_VBUS_SHORT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
		}
		return;
	}

	sm5714_corr_sbu_volt_read(pdic_data, &adc_sbu1, &adc_sbu2,
			SBU_SOURCING_ON);
	if ((adc_sbu1 < 0x04 || adc_sbu2 < 0x04) &&
			(adc_sbu1 > 0x2C || adc_sbu2 > 0x2C)) {
#if !defined(CONFIG_SEC_FACTORY)
		pdic_data->is_sbu_abnormal_state = true;
#endif
		pr_info("%s, SBU-GND SHORT\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_GND_SBU_SHORT_COUNT);
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
		event = NOTIFY_EXTRA_SYSMSG_SBU_GND_SHORT;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
	}
}
#endif

static void sm5714_control_gpio_for_sbu(int onoff)
{
#if !defined(CONFIG_ARCH_QCOM_NO)
	struct otg_notify *o_notify = get_otg_notify();
	struct usb_notifier_platform_data *pdata = get_notify_data(o_notify);

	if (o_notify)
		o_notify->set_ldo_onoff(pdata, onoff);
#endif
}

static void sm5714_check_cc_state(struct sm5714_phydrv_data *pdic_data)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 data = 0, status1 = 0, reg_jig = 0, reg_comp = 0, reg_clk = 0;
	bool abnormal_st = false;

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (!pdic_data->try_state_change)
#elif IS_ENABLED(CONFIG_TYPEC)
	if (!pdic_data->typec_try_state_change)
#endif
	{
		sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS1, &status1);
		sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE0, &data);
		if (!(status1 & SM5714_REG_INT_STATUS1_ATTACH)) {
			if (((data & 0x0F) == 0x1) || /* Set CC_DISABLE to CC_UNATT_SNK */
				(((data & 0x0F) == 0xA) && (status1 & SM5714_REG_INT_STATUS1_VBUSPOK))) {
				/* Set CC_ATTWAIT_SRC to CC_UNATT_SNK */
				sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, 0x82);
			} else /* Set CC_OP_EN */
				sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, 0x80);
		}
	}

	sm5714_usbpd_read_reg(i2c, SM5714_REG_JIGON_CONTROL, &reg_jig);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_COMP_CNTL, &reg_comp);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CLK_CNTL, &reg_clk);

	if ((reg_jig != 0x3 && reg_jig != 0x1) || (reg_comp != 0x98) || (reg_clk != 0x8))
		abnormal_st = true;

	pr_info("%s, CC_ST : 0x%x, JIGON : 0x%x, COMP : 0x%x, CLK : 0x%x\n",
			__func__, data, reg_jig, reg_comp, reg_clk);

	if (abnormal_st) {
		dev_info(dev, "Do soft reset.\n");
		sm5714_usbpd_write_reg(i2c, SM5714_REG_SYS_CNTL, 0x80);
		sm5714_driver_reset(pd_data);
		sm5714_usbpd_reg_init(pdic_data);
	}
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static void sm5714_notify_rp_current_level(void *_data)
{
	struct sm5714_usbpd_data *pd_data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 cc_status = 0, rp_currentlvl = 0;
	bool short_cable = false;
#if defined(CONFIG_TYPEC)
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#endif

	sm5714_get_short_state(pd_data, &short_cable);

	if (short_cable)
		return;
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	if (pdic_data->is_water_detect)
		return;
#endif

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_STATUS, &cc_status);

	pr_info("%s : CC_STATUS = 0x%x\n", __func__, cc_status);

	/* PDIC = SINK */
	if ((cc_status & SM5714_ATTACH_TYPE) == SM5714_ATTACH_SOURCE) {
		if ((cc_status & SM5714_ADV_CURR) == 0x00) {
			/* 5V/0.5A RP charger is detected by PDIC */
			rp_currentlvl = RP_CURRENT_LEVEL_DEFAULT;
		} else if ((cc_status & SM5714_ADV_CURR) == 0x08) {
			/* 5V/1.5A RP charger is detected by PDIC */
			rp_currentlvl = RP_CURRENT_LEVEL2;
#if defined(CONFIG_TYPEC)
			mode = TYPEC_PWR_MODE_1_5A;
#endif
		} else {
			/* 5V/3A RP charger is detected by PDIC */
			rp_currentlvl = RP_CURRENT_LEVEL3;
#if defined(CONFIG_TYPEC)
			mode = TYPEC_PWR_MODE_3_0A;
#endif
		}
		pdic_data->rp_currentlvl = rp_currentlvl;
#if defined(CONFIG_TYPEC)
		if (!pdic_data->pd_support) {
			pdic_data->pwr_opmode = mode;
			typec_set_pwr_opmode(pdic_data->port, mode);
		}
#endif
		if (rp_currentlvl != pd_data->pd_noti.sink_status.rp_currentlvl &&
				rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT &&
				!pdic_data->pd_support) {
			pd_data->pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
			pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PDIC_ATTACH;
			pr_info("%s : rp_currentlvl(%d)\n", __func__,
					pd_data->pd_noti.sink_status.rp_currentlvl);
			/* TODO: Notify to AP - rp_currentlvl */
			sm5714_pdic_event_work(pdic_data,
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
				PDIC_NOTIFY_DEV_BATTERY,
#else
				PDIC_NOTIFY_DEV_BATT,
#endif
				PDIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);

			sm5714_pdic_event_work(pdic_data, PDIC_NOTIFY_DEV_MUIC,
					PDIC_NOTIFY_ID_ATTACH,
					PDIC_NOTIFY_ATTACH,
					USB_STATUS_NOTIFY_DETACH,
					rp_currentlvl);
		}
	}
}

static void sm5714_notify_rp_abnormal(void *_data)
{
	struct sm5714_usbpd_data *pd_data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 cc_status = 0, rp_currentlvl = RP_CURRENT_ABNORMAL;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_STATUS, &cc_status);

	/* PDIC = SINK */
	if ((cc_status & SM5714_ATTACH_TYPE) == SM5714_ATTACH_SOURCE) {
		if (rp_currentlvl != pd_data->pd_noti.sink_status.rp_currentlvl) {
			pd_data->pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
			pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_PDIC_ATTACH;
			pr_info("%s : rp_currentlvl(%d)\n", __func__,
					pd_data->pd_noti.sink_status.rp_currentlvl);

			sm5714_pdic_event_work(pdic_data,
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
				PDIC_NOTIFY_DEV_BATTERY,
#else
				PDIC_NOTIFY_DEV_BATT,
#endif
				PDIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);

			sm5714_pdic_event_work(pdic_data, PDIC_NOTIFY_DEV_MUIC,
					PDIC_NOTIFY_ID_ATTACH,
					PDIC_NOTIFY_ATTACH,
					USB_STATUS_NOTIFY_DETACH,
					rp_currentlvl);
		}
	}
}
#endif

static void sm5714_send_role_swap_message(
	struct sm5714_phydrv_data *usbpd_data, u8 mode)
{
	struct sm5714_usbpd_data *pd_data;

	pd_data = dev_get_drvdata(usbpd_data->dev);
	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return;
	}

	pr_info("%s : send %s\n", __func__,
		mode == POWER_ROLE_SWAP ? "pr_swap" : "dr_swap");
	if (mode == POWER_ROLE_SWAP)
		sm5714_usbpd_inform_event(pd_data, MANAGER_PR_SWAP_REQUEST);
	else
		sm5714_usbpd_inform_event(pd_data, MANAGER_DR_SWAP_REQUEST);
}

void sm5714_rprd_mode_change(struct sm5714_phydrv_data *usbpd_data, u8 mode)
{
	struct i2c_client *i2c = usbpd_data->i2c;

	pr_info("%s : mode=0x%x\n", __func__, mode);

	switch (mode) {
	case TYPE_C_ATTACH_DFP: /* SRC */
		sm5714_set_detach(usbpd_data, mode);
		msleep(1000);
		sm5714_set_attach(usbpd_data, mode);
		break;
	case TYPE_C_ATTACH_UFP: /* SNK */
		sm5714_set_detach(usbpd_data, mode);
		msleep(1000);
		sm5714_set_attach(usbpd_data, mode);
		break;
	case TYPE_C_ATTACH_DRP: /* DRP */
		sm5714_usbpd_write_reg(i2c,
			SM5714_REG_CC_CNTL1, 0x41);
		break;
	};
}

void sm5714_power_role_change(struct sm5714_phydrv_data *usbpd_data,
	int power_role)
{
	pr_info("%s : power_role is %s\n", __func__,
		power_role == TYPE_C_ATTACH_SRC ? "src" : "snk");

	switch (power_role) {
	case TYPE_C_ATTACH_SRC:
	case TYPE_C_ATTACH_SNK:
		sm5714_send_role_swap_message(usbpd_data, POWER_ROLE_SWAP);
		break;
	};
}

void sm5714_data_role_change(struct sm5714_phydrv_data *usbpd_data,
	int data_role)
{
	pr_info("%s : data_role is %s\n", __func__,
		data_role == TYPE_C_ATTACH_DFP ? "dfp" : "ufp");

	switch (data_role) {
	case TYPE_C_ATTACH_DFP:
	case TYPE_C_ATTACH_UFP:
		sm5714_send_role_swap_message(usbpd_data, DATA_ROLE_SWAP);
		break;
	};
}

static void sm5714_role_swap_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct sm5714_phydrv_data *usbpd_data =
		container_of(delay_work,
			struct sm5714_phydrv_data, role_swap_work);

	pr_info("%s: role swap check again.\n", __func__);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	usbpd_data->try_state_change = 0;
#elif defined(CONFIG_TYPEC)
	usbpd_data->typec_try_state_change = 0;
#endif
	if (!usbpd_data->is_attached) {
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		sm5714_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
	}
}

void sm5714_vbus_dischg_work(struct work_struct *work)
{
	struct sm5714_phydrv_data *pdic_data =
		container_of(work, struct sm5714_phydrv_data,
				vbus_dischg_work.work);

	if (gpio_is_valid(pdic_data->vbus_dischg_gpio)) {
		gpio_set_value(pdic_data->vbus_dischg_gpio, 0);
		pr_info("%s vbus_discharging(%d)\n", __func__,
			gpio_get_value(pdic_data->vbus_dischg_gpio));
	}
}

void sm5714_usbpd_set_vbus_dischg_gpio(struct sm5714_phydrv_data
		*pdic_data, int vbus_dischg)
{
	if (!gpio_is_valid(pdic_data->vbus_dischg_gpio))
		return;

	cancel_delayed_work_sync(&pdic_data->vbus_dischg_work);
	gpio_set_value(pdic_data->vbus_dischg_gpio, vbus_dischg);

	if (vbus_dischg > 0)
		schedule_delayed_work(&pdic_data->vbus_dischg_work,
			msecs_to_jiffies(600));

	pr_info("%s vbus_discharging(%d)\n", __func__,
		gpio_get_value(pdic_data->vbus_dischg_gpio));
}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER_SM5714)
static void sm5714_usbpd_handle_vbus(struct work_struct *work)
{
	struct sm5714_phydrv_data *pdic_data =
		container_of(work, struct sm5714_phydrv_data,
				vbus_noti_work.work);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 status1 = 0, status2 = 0;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS1, &status1);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS2, &status2);

	if (status1 & SM5714_REG_INT_STATUS1_VBUSPOK)
		vbus_notifier_handle(STATUS_VBUS_HIGH);
	else if (status2 & SM5714_REG_INT_STATUS2_VBUS_0V)
		vbus_notifier_handle(STATUS_VBUS_LOW);
}
#endif /* CONFIG_VBUS_NOTIFIER */

#if defined(CONFIG_SEC_FACTORY)
static void sm5714_factory_check_abnormal_state(struct work_struct *work)
{
	struct sm5714_phydrv_data *pdic_data =
		container_of(work, struct sm5714_phydrv_data,
				factory_state_work.work);
	int state_cnt = pdic_data->factory_mode.FAC_Abnormal_Repeat_State;

	if (state_cnt >= FAC_ABNORMAL_REPEAT_STATE) {
		pr_info("%s: Notify the abnormal state [STATE] [ %d]",
				__func__, state_cnt);
		/* Notify to AP - ERROR STATE 1 */
		sm5714_pdic_event_work(pdic_data,	PDIC_NOTIFY_DEV_PDIC,
			PDIC_NOTIFY_ID_FAC, 1, 0, 0);
	} else
		pr_info("%s: [STATE] cnt :  [%d]", __func__, state_cnt);
	pdic_data->factory_mode.FAC_Abnormal_Repeat_State = 0;
}

static void sm5714_factory_check_normal_rid(struct work_struct *work)
{
	struct sm5714_phydrv_data *pdic_data =
		container_of(work, struct sm5714_phydrv_data,
				factory_rid_work.work);
	int rid_cnt = pdic_data->factory_mode.FAC_Abnormal_Repeat_RID;

	if (rid_cnt >= FAC_ABNORMAL_REPEAT_RID) {
		pr_info("%s: Notify the abnormal state [RID] [ %d]",
				__func__, rid_cnt);
		/* Notify to AP - ERROR STATE 2 */
		sm5714_pdic_event_work(pdic_data,	PDIC_NOTIFY_DEV_PDIC,
			PDIC_NOTIFY_ID_FAC, 1 << 1, 0, 0);
	} else
		pr_info("%s: [RID] cnt :  [%d]", __func__, rid_cnt);

	pdic_data->factory_mode.FAC_Abnormal_Repeat_RID = 0;
}

static void sm5714_factory_execute_monitor(
		struct sm5714_phydrv_data *usbpd_data, int type)
{
	uint32_t state_cnt = usbpd_data->factory_mode.FAC_Abnormal_Repeat_State;
	uint32_t rid_cnt = usbpd_data->factory_mode.FAC_Abnormal_Repeat_RID;
	uint32_t rid0_cnt = usbpd_data->factory_mode.FAC_Abnormal_RID0;

	pr_info("%s: state_cnt = %d, rid_cnt = %d, rid0_cnt = %d\n",
			__func__, state_cnt, rid_cnt, rid0_cnt);

	switch (type) {
	case FAC_ABNORMAL_REPEAT_RID0:
		if (!rid0_cnt) {
			pr_info("%s: Notify the abnormal state [RID0] [%d]!!",
					__func__, rid0_cnt);
			usbpd_data->factory_mode.FAC_Abnormal_RID0++;
			/* Notify to AP - ERROR STATE 4 */
			sm5714_pdic_event_work(usbpd_data,
				PDIC_NOTIFY_DEV_PDIC,
				PDIC_NOTIFY_ID_FAC, 1 << 2, 0, 0);
		} else {
			usbpd_data->factory_mode.FAC_Abnormal_RID0 = 0;
		}
	break;
	case FAC_ABNORMAL_REPEAT_RID:
		if (!rid_cnt) {
			schedule_delayed_work(&usbpd_data->factory_rid_work,
					msecs_to_jiffies(1000));
			pr_info("%s: start the factory_rid_work", __func__);
		}
		usbpd_data->factory_mode.FAC_Abnormal_Repeat_RID++;
	break;
	case FAC_ABNORMAL_REPEAT_STATE:
		if (!state_cnt) {
			schedule_delayed_work(&usbpd_data->factory_state_work,
					msecs_to_jiffies(1000));
			pr_info("%s: start the factory_state_work", __func__);
		}
		usbpd_data->factory_mode.FAC_Abnormal_Repeat_State++;
	break;
	default:
		pr_info("%s: Never Calling [%d]", __func__, type);
	break;
	}
}
#endif

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
static int sm5714_pdic_set_dual_role(struct dual_role_phy_instance *dual_role,
				enum dual_role_property prop,
				const unsigned int *val)
{
	struct sm5714_phydrv_data *usbpd_data = dual_role_get_drvdata(dual_role);
	struct i2c_client *i2c;
	USB_STATUS attached_state;
	int mode;
	int timeout = 0;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null\n", __func__);
		return -EINVAL;
	}

	i2c = usbpd_data->i2c;

	/* Get Current Role */
	attached_state = usbpd_data->data_role_dual;
	pr_info("%s : request prop = %d , attached_state = %d\n",
			__func__, prop, attached_state);

	if (attached_state != USB_STATUS_NOTIFY_ATTACH_DFP
			&& attached_state != USB_STATUS_NOTIFY_ATTACH_UFP) {
		pr_err("%s : current mode : %d - just return\n",
				__func__, attached_state);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP
			&& *val == DUAL_ROLE_PROP_MODE_DFP) {
		pr_err("%s : current mode : %d - request mode : %d just return\n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP
			&& *val == DUAL_ROLE_PROP_MODE_UFP) {
		pr_err("%s : current mode : %d - request mode : %d just return\n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		/* Current mode DFP and Source  */
		pr_info("%s: try reversing, from Source to Sink\n", __func__);
		/* turns off VBUS first */
		sm5714_vbus_turn_on_ctrl(usbpd_data, 0);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		/* muic */
		sm5714_pdic_event_work(usbpd_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
			PDIC_NOTIFY_DETACH/*attach*/,
			USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
#endif
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_UFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_UFP;
		sm5714_rprd_mode_change(usbpd_data, mode);
	} else {
		/* Current mode UFP and Sink  */
		pr_info("%s: try reversing, from Sink to Source\n", __func__);
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_DFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_DFP;
		sm5714_rprd_mode_change(usbpd_data, mode);
	}

	reinit_completion(&usbpd_data->reverse_completion);
	timeout =
		wait_for_completion_timeout(&usbpd_data->reverse_completion,
					msecs_to_jiffies
					(DUAL_ROLE_SET_MODE_WAIT_MS));

	if (!timeout) {
		usbpd_data->try_state_change = 0;
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		/* exit from Disabled state and set mode to DRP */
		mode =  TYPE_C_ATTACH_DRP;
		sm5714_rprd_mode_change(usbpd_data, mode);
		ret = -EIO;
	} else {
		pr_err("%s: reverse success, one more check\n", __func__);
		schedule_delayed_work(&usbpd_data->role_swap_work,
				msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
	}

	dev_info(&i2c->dev, "%s -> data role : %d\n", __func__, *val);
	return ret;
}

/* Decides whether userspace can change a specific property */
static int sm5714_dual_role_is_writeable(struct dual_role_phy_instance *drp,
				enum dual_role_property prop)
{
	if (prop == DUAL_ROLE_PROP_MODE)
		return 1;
	else
		return 0;
}

static int sm5714_dual_role_get_prop(
		struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop,
		unsigned int *val)
{
	struct sm5714_phydrv_data *usbpd_data = dual_role_get_drvdata(dual_role);

	USB_STATUS attached_state;
	int power_role_dual;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null : request prop = %d\n",
				__func__, prop);
		return -EINVAL;
	}
	attached_state = usbpd_data->data_role_dual;
	power_role_dual = usbpd_data->power_role_dual;

	pr_info("%s : prop = %d , attached_state = %d, power_role_dual = %d\n",
		__func__, prop, attached_state, power_role_dual);

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			return -EINVAL;
	} else if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_UFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_DEVICE;
		else
			return -EINVAL;
	} else {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
			return -EINVAL;
	}

	return 0;
}

static int sm5714_dual_role_set_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop,
			const unsigned int *val)
{
	pr_info("%s : request prop = %d , *val = %d\n", __func__, prop, *val);
	if (prop == DUAL_ROLE_PROP_MODE)
		return sm5714_pdic_set_dual_role(dual_role, prop, val);
	else
		return -EINVAL;
}
#elif defined(CONFIG_TYPEC)
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int sm5714_dr_set(const struct typec_capability *cap,
	enum typec_data_role role)
#else
static int sm5714_dr_set(struct typec_port *port,
	enum typec_data_role role)
#endif
{
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
	struct sm5714_phydrv_data *usbpd_data =
		container_of(cap, struct sm5714_phydrv_data, typec_cap);
#else
	struct sm5714_phydrv_data *usbpd_data = typec_get_drvdata(port);
#endif
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_DR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */
	pr_info("%s : typec_power_role=%d, typec_data_role=%d, role=%d\n",
		__func__, usbpd_data->typec_power_role,
		usbpd_data->typec_data_role, role);

	if (usbpd_data->typec_data_role != TYPEC_DEVICE
		&& usbpd_data->typec_data_role != TYPEC_HOST)
		return -EPERM;
	else if (usbpd_data->typec_data_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_DEVICE) {
		pr_info("%s : try reversing, from DFP to UFP\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		sm5714_data_role_change(usbpd_data, TYPE_C_ATTACH_UFP);
	} else if (role == TYPEC_HOST) {
		pr_info("%s : try reversing, from UFP to DFP\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_DR;
		sm5714_data_role_change(usbpd_data, TYPE_C_ATTACH_DFP);
	} else {
		pr_info("%s : invalid typec_role\n", __func__);
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		return -ETIMEDOUT;
	}

	return 0;
}

#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int sm5714_pr_set(const struct typec_capability *cap,
	enum typec_role role)
#else
static int sm5714_pr_set(struct typec_port *port, enum typec_role role)
#endif
{
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
	struct sm5714_phydrv_data *usbpd_data =
		container_of(cap, struct sm5714_phydrv_data, typec_cap);
#else
		struct sm5714_phydrv_data *usbpd_data = typec_get_drvdata(port);
#endif
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_PR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */
	pr_info("%s : typec_power_role=%d, typec_data_role=%d, role=%d\n",
		__func__, usbpd_data->typec_power_role,
		usbpd_data->typec_data_role, role);

	if (usbpd_data->typec_power_role != TYPEC_SINK
	    && usbpd_data->typec_power_role != TYPEC_SOURCE)
		return -EPERM;
	else if (usbpd_data->typec_power_role == role)
		return -EPERM;

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (role == TYPEC_SINK) {
		pr_info("%s : try reversing, from Source to Sink\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		sm5714_power_role_change(usbpd_data, TYPE_C_ATTACH_SNK);
	} else if (role == TYPEC_SOURCE) {
		pr_info("%s : try reversing, from Sink to Source\n", __func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_PR;
		sm5714_power_role_change(usbpd_data, TYPE_C_ATTACH_SRC);
	} else {
		pr_info("%s : invalid typec_role\n", __func__);
		return -EIO;
	}
	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(TRY_ROLE_SWAP_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		if (usbpd_data->typec_power_role != role)
			return -ETIMEDOUT;
	}

	return 0;
}

#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int sm5714_port_type_set(const struct typec_capability *cap,
	enum typec_port_type port_type)
#else
static int sm5714_port_type_set(struct typec_port *port,
	enum typec_port_type port_type)
#endif
{
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
	struct sm5714_phydrv_data *usbpd_data =
		container_of(cap, struct sm5714_phydrv_data, typec_cap);
#else
	struct sm5714_phydrv_data *usbpd_data = typec_get_drvdata(port);
#endif

	pr_info("%s : typec_power_role=%d, typec_data_role=%d, port_type=%d\n",
		__func__, usbpd_data->typec_power_role,
		usbpd_data->typec_data_role, port_type);

	reinit_completion(&usbpd_data->typec_reverse_completion);
	if (port_type == TYPEC_PORT_DFP) {
		pr_info("%s : try reversing, from UFP(Sink) to DFP(Source)\n",
			__func__);
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		sm5714_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DFP);
	} else if (port_type == TYPEC_PORT_UFP) {
		pr_info("%s : try reversing, from DFP(Source) to UFP(Sink)\n",
			__func__);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		sm5714_pdic_event_work(usbpd_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
			0/*attach*/, 0/*rprd*/, 0);
#endif
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		sm5714_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_UFP);
	} else {
		pr_info("%s : invalid typec_role\n", __func__);
		return 0;
	}

	if (!wait_for_completion_timeout(&usbpd_data->typec_reverse_completion,
				msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS))) {
		usbpd_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		/* exit from Disabled state and set mode to DRP */
		sm5714_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
		return -ETIMEDOUT;
	} else {
		pr_err("%s: reverse success, one more check\n", __func__);
		schedule_delayed_work(&usbpd_data->role_swap_work,
				msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
	}

	return 0;
}

#if defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
static const struct typec_operations sm5714_typec_ops = {
	.dr_set = sm5714_dr_set,
	.pr_set = sm5714_pr_set,
	.port_type_set = sm5714_port_type_set
};
#endif

int sm5714_get_pd_support(struct sm5714_phydrv_data *usbpd_data)
{
	bool support_pd_role_swap = false;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, "sm5714-usbpd");

	if (np)
		support_pd_role_swap =
			of_property_read_bool(np, "support_pd_role_swap");
	else
		pr_info("%s : np is null\n", __func__);

	pr_info("%s : support_pd_role_swap is %d, pd_support : %d\n",
		__func__, support_pd_role_swap, usbpd_data->pd_support);

	if (support_pd_role_swap && usbpd_data->pd_support)
		return TYPEC_PWR_MODE_PD;

	return usbpd_data->pwr_opmode;
}
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static void sm5714_pdic_event_notifier(struct work_struct *data)
{
	struct pdic_state_work *event_work =
		container_of(data, struct pdic_state_work, pdic_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct sm5714_usbpd_data *pd_data = g_pd_data;
#endif
	PD_NOTI_TYPEDEF pdic_noti;

	switch (event_work->dest) {
	case PDIC_NOTIFY_DEV_USB:
		pr_info("%s, dest=%s, id=%s, attach=%s, drp=%s, event_work=%p\n",
			__func__,
			pdic_event_dest_string(event_work->dest),
			pdic_event_id_string(event_work->id),
			event_work->attach ? "Attached" : "Detached",
			pdic_usbstatus_string(event_work->event),
			event_work);
		break;
	default:
		pr_info("%s, dest=%s, id=%s, attach=%d, event=%d, event_work=%p\n",
			__func__,
			pdic_event_dest_string(event_work->dest),
			pdic_event_id_string(event_work->id),
			event_work->attach,
			event_work->event,
			event_work);
		break;
	}

	pdic_noti.src = PDIC_NOTIFY_DEV_PDIC;
	pdic_noti.dest = event_work->dest;
	pdic_noti.id = event_work->id;
	pdic_noti.sub1 = event_work->attach;
	pdic_noti.sub2 = event_work->event;
	pdic_noti.sub3 = event_work->sub;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdic_noti.pd = &pd_data->pd_noti;
#endif
	pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, NULL, 0);

	kfree(event_work);
}

void sm5714_pdic_event_work(void *data, int dest,
		int id, int attach, int event, int sub)
{
	struct sm5714_phydrv_data *usbpd_data = data;
	struct pdic_state_work *event_work;
#if defined(CONFIG_TYPEC)
	struct typec_partner_desc desc;
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#endif

	pr_info("%s : usb: DIAES %d-%d-%d-%d-%d\n",
		__func__, dest, id, attach, event, sub);
	event_work = kmalloc(sizeof(struct pdic_state_work), GFP_ATOMIC);
	if (!event_work) {
		pr_err("%s: failed to allocate event_work\n", __func__);
		return;
	}
	INIT_WORK(&event_work->pdic_work, sm5714_pdic_event_notifier);

	event_work->dest = dest;
	event_work->id = id;
	event_work->attach = attach;
	event_work->event = event;
	event_work->sub = sub;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (id == PDIC_NOTIFY_ID_USB) {
		pr_info("usb: %s, dest=%d, event=%d, try_state_change=%d\n",
			__func__, dest, event, usbpd_data->try_state_change);

		usbpd_data->data_role_dual = event;

		if (usbpd_data->dual_role != NULL)
			dual_role_instance_changed(usbpd_data->dual_role);

		if (usbpd_data->try_state_change &&
				(usbpd_data->data_role_dual !=
				USB_STATUS_NOTIFY_DETACH)) {
			pr_info("usb: %s, reverse_completion\n", __func__);
			complete(&usbpd_data->reverse_completion);
		}
	}
#elif defined(CONFIG_TYPEC)
	if (id == PDIC_NOTIFY_ID_USB) {
		pr_info("%s : typec_power_role=%d typec_data_role=%d, event=%d\n",
			__func__, usbpd_data->typec_power_role,
			usbpd_data->typec_data_role, event);
		if (usbpd_data->partner == NULL) {
			if (event == USB_STATUS_NOTIFY_ATTACH_UFP) {
				mode = sm5714_get_pd_support(usbpd_data);
				typec_set_pwr_opmode(usbpd_data->port, mode);
				desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
				desc.accessory = TYPEC_ACCESSORY_NONE;
				desc.identity = NULL;
				usbpd_data->typec_data_role = TYPEC_DEVICE;
				typec_set_pwr_role(usbpd_data->port,
					usbpd_data->typec_power_role);
				typec_set_data_role(usbpd_data->port,
					usbpd_data->typec_data_role);
				usbpd_data->partner = typec_register_partner(usbpd_data->port,
					&desc);
			} else if (event == USB_STATUS_NOTIFY_ATTACH_DFP) {
				mode = sm5714_get_pd_support(usbpd_data);
				typec_set_pwr_opmode(usbpd_data->port, mode);
				desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
				desc.accessory = TYPEC_ACCESSORY_NONE;
				desc.identity = NULL;
				usbpd_data->typec_data_role = TYPEC_HOST;
				typec_set_pwr_role(usbpd_data->port,
					usbpd_data->typec_power_role);
				typec_set_data_role(usbpd_data->port,
					usbpd_data->typec_data_role);
				usbpd_data->partner = typec_register_partner(usbpd_data->port,
					&desc);
			} else
				pr_info("%s : detach case\n", __func__);
			if (usbpd_data->typec_try_state_change &&
					(event != USB_STATUS_NOTIFY_DETACH)) {
				pr_info("usb: %s, typec_reverse_completion\n", __func__);
				complete(&usbpd_data->typec_reverse_completion);
			}
		} else {
			if (event == USB_STATUS_NOTIFY_ATTACH_UFP) {
				usbpd_data->typec_data_role = TYPEC_DEVICE;
				typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
			} else if (event == USB_STATUS_NOTIFY_ATTACH_DFP) {
				usbpd_data->typec_data_role = TYPEC_HOST;
				typec_set_data_role(usbpd_data->port, usbpd_data->typec_data_role);
			} else
				pr_info("%s : detach case\n", __func__);
		}
	}
#endif
	if (!queue_work(usbpd_data->pdic_wq, &event_work->pdic_work)) {
		pr_info("usb: %s, event_work(%p) is dropped\n",
			__func__, event_work);
		kfree(event_work);
	}
}

static void sm5714_process_dr_swap(struct sm5714_phydrv_data *usbpd_data, int val)
{
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();

	send_otg_notify(o_notify, NOTIFY_EVENT_DR_SWAP, 1);
#endif

	if (val == USBPD_UFP) {
		sm5714_pdic_event_work(usbpd_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				PDIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		/* muic */
		sm5714_pdic_event_work(usbpd_data,
				PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
				PDIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_UFP/*rprd*/, 0);
		sm5714_pdic_event_work(usbpd_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				PDIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
	} else if (val == USBPD_DFP) {
		sm5714_pdic_event_work(usbpd_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				PDIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		/* muic */
		sm5714_pdic_event_work(usbpd_data,
				PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
				PDIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_DFP/*rprd*/, 0);
		sm5714_pdic_event_work(usbpd_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				PDIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/, 0);
	} else
		pr_err("%s : invalid val\n", __func__);
}

static void sm5714_control_option_command(
		struct sm5714_phydrv_data *pdic_data, int cmd)
{
	struct sm5714_usbpd_data *_data = dev_get_drvdata(pdic_data->dev);
	int pd_cmd = cmd & 0x0f;

	switch (pd_cmd) {
	/* 0x1 : Vconn control option command ON */
	case 1:
		sm5714_set_vconn_source(_data, USBPD_VCONN_ON);
		break;
	/* 0x2 : Vconn control option command OFF */
	case 2:
		sm5714_set_vconn_source(_data, USBPD_VCONN_OFF);
		break;
	default:
		break;
	}
}

static int sm5714_sysfs_get_prop(struct _pdic_data_t *ppdic_data,
					enum pdic_sysfs_property prop,
					char *buf)
{
	int retval = -ENODEV;
	struct sm5714_phydrv_data *usbpd_data =
			(struct sm5714_phydrv_data *)ppdic_data->drv_data;
	struct sm5714_usbpd_data *pd_data;
	struct sm5714_usbpd_manager_data *manager;
	u8 adc_sbu1 = 0, adc_sbu2 = 0;
#if defined(CONFIG_SEC_FACTORY)
	u8 cc_status = 0;
#endif

	if (!usbpd_data) {
		pr_info("%s : usbpd_data is null\n", __func__);
		return retval;
	}
	pd_data = dev_get_drvdata(usbpd_data->dev);
	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return retval;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s : manager is null\n", __func__);
		return retval;
	}

	switch (prop) {
	case PDIC_SYSFS_PROP_LPM_MODE:
		retval = sprintf(buf, "%d\n", usbpd_data->lpm_mode);
		pr_info("%s : PDIC_SYSFS_PROP_LPM_MODE : %s", __func__, buf);
		break;
	case PDIC_SYSFS_PROP_RID:
		retval = sprintf(buf, "%d\n", usbpd_data->rid == REG_RID_MAX ?
				REG_RID_OPEN : usbpd_data->rid);
		pr_info("%s : PDIC_SYSFS_PROP_RID : %s", __func__, buf);
		break;
	case PDIC_SYSFS_PROP_FW_WATER:
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
		retval = sprintf(buf, "%d\n", usbpd_data->is_water_detect);
#else
		retval = sprintf(buf, "0\n");
#endif
		pr_info("%s : PDIC_SYSFS_PROP_FW_WATER : %s", __func__, buf);
		break;
	case PDIC_SYSFS_PROP_STATE:
		retval = sprintf(buf, "%d\n", (int)pd_data->policy.plug_valid);
		pr_info("%s : PDIC_SYSFS_PROP_STATE : %s", __func__, buf);
		break;
	case PDIC_SYSFS_PROP_ACC_DEVICE_VERSION:
		retval = sprintf(buf, "%04x\n", manager->Device_Version);
		pr_info("%s : PDIC_SYSFS_PROP_ACC_DEVICE_VERSION : %s",
				__func__, buf);
		break;
	case PDIC_SYSFS_PROP_CONTROL_GPIO:
		sm5714_corr_sbu_volt_read(usbpd_data, &adc_sbu1, &adc_sbu2,
				SBU_SOURCING_OFF);

		pr_info("%s : PDIC_SYSFS_PROP_CONTROL_GPIO SBU1 = 0x%x ,SBU2 = 0x%x",
				__func__, adc_sbu1, adc_sbu2);
		if (adc_sbu1 < 0x3c)
			adc_sbu1 = 0;
		else
			adc_sbu1 = 1;
		if (adc_sbu2 < 0x3c)
			adc_sbu2 = 0;
		else
			adc_sbu2 = 1;
		retval = sprintf(buf, "%d %d\n", adc_sbu1, adc_sbu2);
		break;
	case PDIC_SYSFS_PROP_USBPD_IDS:
		retval = sprintf(buf, "%04x:%04x\n",
				le16_to_cpu(manager->Vendor_ID),
				le16_to_cpu(manager->Product_ID));
		pr_info("%s : PDIC_SYSFS_PROP_USBPD_IDS : %s", __func__, buf);
		break;
	case PDIC_SYSFS_PROP_USBPD_TYPE:
		retval = sprintf(buf, "%d\n", manager->acc_type);
		pr_info("%s : PDIC_SYSFS_PROP_USBPD_TYPE : %s", __func__, buf);
		break;
#if defined(CONFIG_SEC_FACTORY)
	case PDIC_SYSFS_PROP_CC_PIN_STATUS:
		sm5714_usbpd_read_reg(usbpd_data->i2c, SM5714_REG_CC_STATUS, &cc_status);
		if (cc_status & 0x7) { //2:0bit ATTACH_TYPE
			if (cc_status & 0x20) //5bit CABLE_FLIP
				retval = sprintf(buf, "2\n"); //CC2_ACTVIE
			else
				retval = sprintf(buf, "1\n"); //CC1_ACTIVE
		} else
			retval = sprintf(buf, "0\n"); //NO_DETERMINATION
		pr_info("%s : PDIC_SYSFS_PROP_CC_PIN_STATUS : %s", __func__, buf);
		break;
	case PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE:
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
		retval = sprintf(buf, "sysfs\n");
#else
		retval = sprintf(buf, "unsupport\n");
#endif
		pr_info("%s : PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE : %s", __func__, buf);
		break;
        case PDIC_SYSFS_PROP_VBUS_ADC:
		manager->vbus_adc = sm5714_vbus_adc_read(usbpd_data);
		retval = sprintf(buf, "%d\n", manager->vbus_adc);
		pr_info("%s : PDIC_SYSFS_PROP_VBUS_ADC : %s", __func__, buf);
		break;
#endif
	default:
		pr_info("%s : prop read not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		break;
	}
	return retval;
}

static ssize_t sm5714_sysfs_set_prop(struct _pdic_data_t *ppdic_data,
				enum pdic_sysfs_property prop,
				const char *buf, size_t size)
{
	ssize_t retval = size;
	struct sm5714_phydrv_data *usbpd_data =
			(struct sm5714_phydrv_data *)ppdic_data->drv_data;
	int rv;
	int mode = 0;

	if (!usbpd_data) {
		pr_info("%s : usbpd_data is null : request prop = %d\n",
				__func__, prop);
		return -ENODEV;
	}

	switch (prop) {
	case PDIC_SYSFS_PROP_LPM_MODE:
		rv = sscanf(buf, "%d", &mode);
		pr_info("%s : PDIC_SYSFS_PROP_LPM_MODE mode=%d\n",
				__func__, mode);
		mutex_lock(&usbpd_data->lpm_mutex);
		if (mode == 1 || mode == 2)
			sm5714_set_lpm_mode(usbpd_data);
		else
			sm5714_set_normal_mode(usbpd_data);
		mutex_unlock(&usbpd_data->lpm_mutex);
		break;
	case PDIC_SYSFS_PROP_CTRL_OPTION:
		rv = sscanf(buf, "%d", &mode);
		pr_info("%s : PDIC_SYSFS_PROP_CTRL_OPTION mode=%d\n",
				__func__, mode);
		sm5714_control_option_command(usbpd_data, mode);
		break;
	case PDIC_SYSFS_PROP_CONTROL_GPIO:
		rv = sscanf(buf, "%d", &mode);
		pr_info("%s : PDIC_SYSFS_PROP_CONTROL_GPIO mode=%d. do nothing for control gpio.\n",
				__func__, mode);
		/* original concept : mode 0 : SBU1/SBU2 set as open-drain status
		 *                         mode 1 : SBU1/SBU2 set as default status - Pull up
		 *  But, SM5714 is always open-drain status so we don't need to control it.
		 */
		sm5714_control_gpio_for_sbu(!mode);
		break;
	default:
		pr_info("%s : prop write not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		return retval;
	}
	return size;
}

static int sm5714_sysfs_is_writeable(struct _pdic_data_t *ppdic_data,
				enum pdic_sysfs_property prop)
{
	switch (prop) {
	case PDIC_SYSFS_PROP_LPM_MODE:
	case PDIC_SYSFS_PROP_CTRL_OPTION:
	case PDIC_SYSFS_PROP_CONTROL_GPIO:
		return 1;
	default:
		return 0;
	}
}
#endif

#ifndef CONFIG_SEC_FACTORY
void sm5714_usbpd_set_rp_scr_sel(struct sm5714_usbpd_data *_data, int scr_sel)
{
	struct sm5714_phydrv_data *pdic_data = _data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	pr_info("%s: scr_sel : (%d)\n", __func__, scr_sel);
	pdic_data->scr_sel = scr_sel;

	switch (scr_sel) {
	case PLUG_CTRL_RP80:
		sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL1, &data);
		data &= 0xCF;
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL1, data);
		break;
	case PLUG_CTRL_RP180:
		sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL1, &data);
		data |= 0x10;
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL1, data);
		break;
	case PLUG_CTRL_RP330:
		sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL1, &data);
		data |= 0x20;
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL1, data);
		break;
	default:
		break;
	}
}
#endif

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
static int sm5714_usbpd_sbu_test_read(void *data)
{
	struct sm5714_phydrv_data *usbpd_data = data;
	struct i2c_client *test_i2c;
	u8 adc_sbu1, adc_sbu2;

	if (usbpd_data == NULL)
		return -ENXIO;

	test_i2c = usbpd_data->i2c;

	sm5714_usbpd_write_reg(test_i2c, SM5714_REG_CORR_CNTL5, 0x98);
	sm5714_usbpd_write_reg(test_i2c, SM5714_REG_CORR_CNTL6, 0x40);
	usleep_range(5000, 5100);

	sm5714_usbpd_write_reg(test_i2c, SM5714_REG_ADC_CNTL1,
			SM5714_ADC_PATH_SEL_SBU1);
	sm5714_adc_value_read(usbpd_data, &adc_sbu1);

	sm5714_usbpd_write_reg(test_i2c, SM5714_REG_ADC_CNTL1,
			SM5714_ADC_PATH_SEL_SBU2);
	sm5714_adc_value_read(usbpd_data, &adc_sbu2);

	sm5714_usbpd_write_reg(test_i2c, SM5714_REG_CORR_CNTL5, 0x00);
	sm5714_usbpd_write_reg(test_i2c, SM5714_REG_CORR_CNTL6, 0x00);

	pr_info("%s, SBU1_VOLT : 0x%x, SBU2_VOLT : 0x%x\n",
			__func__, adc_sbu1, adc_sbu2);
	if (adc_sbu1 < 0x10 || adc_sbu2 < 0x10)
		return 0;
	else
		return 1;
}

void sm5714_usbpd_set_host_on(void *data, int mode)
{
	struct sm5714_phydrv_data *usbpd_data = data;

	if (!usbpd_data)
		return;

	pr_info("%s : current_set is %d!\n", __func__, mode);
	if (mode) {
		usbpd_data->detach_done_wait = 0;
		usbpd_data->host_turn_on_event = 1;
		wake_up_interruptible(&usbpd_data->host_turn_on_wait_q);
	} else {
		usbpd_data->detach_done_wait = 0;
		usbpd_data->host_turn_on_event = 0;
	}
}
#endif

static int sm5714_write_msg_header(struct i2c_client *i2c, u8 *buf)
{
	int ret;

	ret = sm5714_usbpd_multi_write(i2c, SM5714_REG_TX_HEADER_00, 2, buf);
	return ret;
}

static int sm5714_write_msg_obj(struct i2c_client *i2c,
		int count, data_obj_type *obj)
{
	int ret = 0;
	int i = 0, j = 0;
	u8 reg[USBPD_MAX_COUNT_RX_PAYLOAD] = {0, };
	struct device *dev = &i2c->dev;

	if (count > SM5714_MAX_NUM_MSG_OBJ)
		dev_err(dev, "%s, not invalid obj count number\n", __func__);
	else {
		for (i = 0; i < (count * 4); i++) {
			if ((i != 0) && (i % 4 == 0))
				j++;
			reg[i] = obj[j].byte[i % 4];
		}
			ret = sm5714_usbpd_multi_write(i2c,
			SM5714_REG_TX_PAYLOAD, (count * 4), reg);
		}
	return ret;
}

static int sm5714_send_msg(void *_data, struct i2c_client *i2c)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_policy_data *policy = &data->policy;
	int ret;
	u8 val;

	if (policy->origin_message == 0x00)
		val = SM5714_REG_MSG_SEND_TX_SOP_REQ;
	else if (policy->origin_message == 0x01)
		val = SM5714_REG_MSG_SEND_TX_SOPP_REQ;
	else
		val = SM5714_REG_MSG_SEND_TX_SOPPP_REQ;

	pr_info("%s, TX_REQ : %x\n", __func__, val);
	ret = sm5714_usbpd_write_reg(i2c, SM5714_REG_TX_REQ, val);

	return ret;
}

static int sm5714_read_msg_header(struct i2c_client *i2c,
		msg_header_type *header)
{
	int ret;

	ret = sm5714_usbpd_multi_read(i2c,
			SM5714_REG_RX_HEADER_00, 2, header->byte);

	return ret;
}

static int sm5714_read_msg_obj(struct i2c_client *i2c,
		int count, data_obj_type *obj)
{
	int ret = 0;
	int i = 0, j = 0;
	u8 reg[USBPD_MAX_COUNT_RX_PAYLOAD] = {0, };
	struct device *dev = &i2c->dev;

	if (count > SM5714_MAX_NUM_MSG_OBJ) {
		dev_err(dev, "%s, not invalid obj count number\n", __func__);
		ret = -EINVAL;
	} else {
		ret = sm5714_usbpd_multi_read(i2c,
			SM5714_REG_RX_PAYLOAD, (count * 4), reg);
		for (i = 0; i < (count * 4); i++) {
			if ((i != 0) && (i % 4 == 0))
				j++;
			obj[j].byte[i % 4] = reg[i];
		}
	}

	return ret;
}

static void sm5714_set_irq_enable(struct sm5714_phydrv_data *_data,
		u8 int0, u8 int1, u8 int2, u8 int3, u8 int4)
{
	u8 int_mask[5]
		= {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	int ret = 0;
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;

	int_mask[0] &= ~int0;
	int_mask[1] &= ~int1;
	int_mask[2] &= ~int2;
	int_mask[3] &= ~int3;
	int_mask[4] &= ~int4;

	ret = sm5714_usbpd_multi_write(i2c, SM5714_REG_INT_MASK1,
			5, int_mask);

	if (ret < 0)
		dev_err(dev, "err write interrupt mask\n");
}

static void sm5714_driver_reset(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	int i;

	pdic_data->status_reg = 0;
	data->wait_for_msg_arrived = 0;
	pdic_data->header.word = 0;
	for (i = 0; i < SM5714_MAX_NUM_MSG_OBJ; i++)
		pdic_data->obj[i].object = 0;

	sm5714_set_irq_enable(pdic_data, ENABLED_INT_1, ENABLED_INT_2,
			ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
}

void sm5714_protocol_layer_reset(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 status2, status5;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS2, &status2);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS5, &status5);

	if ((status2 & SM5714_REG_INT_STATUS2_PD_RID_DETECT) ||
			(status5 & SM5714_REG_INT_STATUS5_JIG_CASE_ON)) {
		pr_info("%s: Do not protocol reset.\n", __func__);
		return;
	}

	/* Rx Buffer Flushing */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_RX_BUF_ST, 0x10);

	/* Reset Protocol Layer */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL4,
			SM5714_REG_CNTL_PROTOCOL_RESET_MESSAGE);

	pr_info("%s\n", __func__);
}

void sm5714_cc_state_hold_on_off(void *_data, int onoff)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL3, &val);
	if (onoff == 1) {
		val &= 0xCF;
		val |= 0x10;
	} else if (onoff == 2) {
		val &= 0xCF;
		val |= 0x20;
	} else
		val &= 0xCF;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, val);
	pr_info("%s: CC State Hold [%d], val = %x\n", __func__, onoff, val);
}

static void sm5714_abnormal_dev_int_on_off(struct sm5714_phydrv_data *pdic_data, int onoff)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, 0xEE, &val);
	if (onoff == 1)
		val |= 0x2;
	else
		val &= 0xFD;
	sm5714_usbpd_write_reg(i2c, 0xEE, val);
	pr_info("%s: ABNORMAL_DEV INT [%d], val = 0x%x\n", __func__, onoff, val);
}

bool sm5714_get_rx_buf_st(void *_data)
{
	struct sm5714_usbpd_data *pd_data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_RX_BUF_ST, &val);
	pr_info("%s: RX_BUF_ST [0x%02X]\n", __func__, val);

	if (val & 0x04) /* Rx Buffer Empty */
		return false;
	else
		return true;
}

void sm5714_src_transition_to_default(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct sm5714_usbpd_manager_data *manager = &data->manager;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL2, &val);
	val &= 0xEF;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, val); /* BIST Off */

	sm5714_set_vconn_source(data, USBPD_VCONN_OFF);
	sm5714_vbus_turn_on_ctrl(pdic_data, 0);
	if (manager->dp_is_connect == 1) {
		pdic_data->detach_done_wait = 1;
		sm5714_usbpd_dp_detach(pdic_data->dev);
		manager->alt_sended = 0;
		manager->vdm_en = 0;
	}
	sm5714_set_dfp(i2c);
	sm5714_set_enable_pd_function(data, PD_ENABLE);
	pdic_data->data_role = USBPD_DFP;
	pdic_data->pd_support = 0;

	if (!sm5714_check_vbus_state(data))
		sm5714_usbpd_kick_policy_work(pdic_data->dev);

	dev_info(pdic_data->dev, "%s\n", __func__);
}

void sm5714_src_transition_to_pwr_on(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;

	sm5714_set_vconn_source(data, USBPD_VCONN_ON);
	sm5714_vbus_turn_on_ctrl(pdic_data, 1);

	/* Hard Reset Done Notify to PRL */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL4,
			SM5714_ATTACH_SOURCE);
	dev_info(pdic_data->dev, "%s\n", __func__);
}

void sm5714_snk_transition_to_default(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct sm5714_usbpd_manager_data *manager = &data->manager;
	struct i2c_client *i2c = pdic_data->i2c;

	sm5714_set_vconn_source(data, USBPD_VCONN_OFF);
	if (manager->dp_is_connect == 1) {
		pdic_data->detach_done_wait = 1;
		sm5714_usbpd_dp_detach(pdic_data->dev);
		manager->alt_sended = 0;
		manager->vdm_en = 0;
	}
	sm5714_set_ufp(i2c);
	pdic_data->data_role = USBPD_UFP;
	pdic_data->pd_support = 0;

	/* Hard Reset Done Notify to PRL */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL4,
			SM5714_ATTACH_SOURCE);
	dev_info(pdic_data->dev, "%s\n", __func__);
}

bool sm5714_check_vbus_state(void *_data)
{
	struct sm5714_usbpd_data *pd_data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PROBE0, &val);
	if (val & 0x40) /* VBUS OK */
		return true;
	else
		return false;
}

static void sm5714_usbpd_abnormal_reset_check(struct sm5714_phydrv_data *pdic_data)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL1, &reg_data);
	pr_info("%s, CC_CNTL1 : 0x%x\n", __func__, reg_data);

	if (reg_data == 0x45) { /* surge reset */
		sm5714_driver_reset(pd_data);
		sm5714_usbpd_reg_init(pdic_data);
	}
}

#ifndef CONFIG_SEC_FACTORY
static int sm5714_usbpd_check_normal_audio_device(struct sm5714_phydrv_data *pdic_data)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 adc_cc1 = 0, adc_cc2 = 0;
	int ret = 0;

	sm5714_usbpd_set_rp_scr_sel(pd_data, PLUG_CTRL_RP180);

	sm5714_usbpd_write_reg(i2c, SM5714_REG_ADC_CNTL1, SM5714_ADC_PATH_SEL_CC1);
	sm5714_adc_value_read(pdic_data, &adc_cc1);

	sm5714_usbpd_write_reg(i2c, SM5714_REG_ADC_CNTL1, SM5714_ADC_PATH_SEL_CC2);
	sm5714_adc_value_read(pdic_data, &adc_cc2);

	if ((adc_cc1 <= 0xF) && (adc_cc2 <= 0xF)) { /* Ra/Ra */
		sm5714_usbpd_set_rp_scr_sel(pd_data, PLUG_CTRL_RP80);
		ret = 1;
	}

	pr_info("%s, CC1 : 0x%x, CC2 : 0x%x, ret = %d\n", __func__, adc_cc1, adc_cc2, ret);

	return ret;
}
#endif

#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
static void sm5714_usbpd_check_normal_otg_device(struct sm5714_phydrv_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 cc1_80ua, cc2_80ua, cc1_fe, cc2_fe, reg_data;
	bool abnormal_st = false;
	int reTry = 0;

	if (pdic_data->abnormal_dev_cnt < 2)
		return;
	else if (pdic_data->abnormal_dev_cnt > 2) {
		sm5714_abnormal_dev_int_on_off(pdic_data, 0);
		/* Timer set 1s */
		sm5714_usbpd_write_reg(i2c, SM5714_REG_GEN_TMR_L, 0x10);
		sm5714_usbpd_write_reg(i2c, SM5714_REG_GEN_TMR_U, 0x27);
		pdic_data->is_timer_expired = true;
		return;
	}

	sm5714_usbpd_write_reg(i2c, 0xFE, 0x02);

	sm5714_usbpd_read_reg(i2c, 0x18, &cc1_80ua);
	sm5714_usbpd_read_reg(i2c, 0x19, &cc2_80ua);
	sm5714_usbpd_read_reg(i2c, 0x1F, &cc1_fe);
	sm5714_usbpd_read_reg(i2c, 0x21, &cc2_fe);

	sm5714_usbpd_write_reg(i2c, 0xFE, 0x00);

	sm5714_usbpd_read_reg(i2c, 0xFE, &reg_data);
	for (reTry = 0; reTry < 5; reTry++) {
		if (reg_data != 0x00) {
			usleep_range(1000, 1100);
			pr_info("%s, 0xFE : 0x%x, retry = %d\n", __func__, reg_data, reTry+1);
			sm5714_usbpd_write_reg(i2c, 0xFE, 0x00);
			sm5714_usbpd_read_reg(i2c, 0xFE, &reg_data);
		} else
			break;
	}
	if (reg_data != 0x00)
		panic("sm5714 usbpd i2c error!!");
	if ((cc1_80ua >= 0xC) && (cc1_80ua <= 0x16) && (cc1_fe < 0x4))
		abnormal_st = true;
	else if ((cc2_80ua >= 0xC) && (cc2_80ua <= 0x16) && (cc2_fe < 0x4))
		abnormal_st = true;

	if (abnormal_st) {
		sm5714_usbpd_write_reg(i2c, 0x93, 0x04);
		sm5714_usbpd_write_reg(i2c, 0x93, 0x00);
		pdic_data->abnormal_dev_cnt = 0;
	}

	pr_info("%s, ABNOR_ST : %d, CC1_80uA[0x%x] CC2_80uA[0x%x] CC1_FE[0x%x] CC2_FE[0x%x]\n",
			__func__, abnormal_st, cc1_80ua, cc2_80ua, cc1_fe, cc2_fe);
}
#endif

static void sm5714_assert_rd(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL7, &val);

	val ^= 0x01;
	/* Apply CC State PR_Swap (Att.Src -> Att.Snk) */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL7, val);
}

static void sm5714_assert_rp(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL7, &val);

	val ^= 0x01;
	/* Apply CC State PR_Swap (Att.Snk -> Att.Src) */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL7, val);
}

static unsigned int sm5714_get_status(void *_data, unsigned int flag)
{
	unsigned int ret;
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;

	if (pdic_data->status_reg & flag) {
		ret = pdic_data->status_reg & flag;
		dev_info(pdic_data->dev, "%s: status_reg = (%x)\n",
				__func__, ret);
		pdic_data->status_reg &= ~flag; /* clear the flag */
		return ret;
	} else {
		return 0;
	}
}

static bool sm5714_poll_status(void *_data, int irq)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[5] = {0};
	u8 status[5] = {0};
	int ret = 0;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
	int event = 0;
#endif

	ret = sm5714_usbpd_multi_read(i2c, SM5714_REG_INT1, 5, intr);
	ret = sm5714_usbpd_multi_read(i2c, SM5714_REG_STATUS1, 5, status);

	if (irq == (-1)) {
		intr[0] = (status[0] & ENABLED_INT_1);
		intr[1] = (status[1] & ENABLED_INT_2);
		intr[2] = (status[2] & ENABLED_INT_3);
		intr[3] = (status[3] & ENABLED_INT_4);
		intr[4] = (status[4] & ENABLED_INT_5);
	}

	dev_info(dev, "%s: INT[0x%x 0x%x 0x%x 0x%x 0x%x], STATUS[0x%x 0x%x 0x%x 0x%x 0x%x]\n",
		__func__, intr[0], intr[1], intr[2], intr[3], intr[4],
		status[0], status[1], status[2], status[3], status[4]);

	if ((intr[0] | intr[1] | intr[2] | intr[3] | intr[4]) == 0) {
		sm5714_usbpd_abnormal_reset_check(pdic_data);
		pdic_data->status_reg |= MSG_NONE;
		goto out;
	}
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	if (intr[0] & SM5714_REG_INT_STATUS1_TMR_EXP) {
		if (pdic_data->is_timer_expired) {
			pdic_data->is_timer_expired = false;
			if (pdic_data->abnormal_dev_cnt != 0)
				pdic_data->abnormal_dev_cnt = 0;
			sm5714_abnormal_dev_int_on_off(pdic_data, 1);
		}
	}
	if (intr[0]	& SM5714_REG_INT_STATUS1_ABNORMAL_DEV) {
		pdic_data->abnormal_dev_cnt++;
		sm5714_usbpd_check_normal_otg_device(pdic_data);
	}
#endif
	if ((intr[4] & SM5714_REG_INT_STATUS5_CC_ABNORMAL) &&
			(status[4] & SM5714_REG_INT_STATUS5_CC_ABNORMAL)) {
		pdic_data->is_cc_abnormal_state = true;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if ((status[0] & SM5714_REG_INT_STATUS1_ATTACH) &&
				pdic_data->is_attached) {
			/* rp abnormal */
			sm5714_notify_rp_abnormal(_data);
		}
#endif
		pr_info("%s, CC-VBUS SHORT\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_VBUS_CC_SHORT_COUNT);
#endif
#if defined(CONFIG_USB_NOTIFY_PROC_LOG)
		event = NOTIFY_EXTRA_SYSMSG_CC_SHORT;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
	}

#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	if ((intr[2] & SM5714_REG_INT_STATUS3_WATER) &&
			(status[2] & SM5714_REG_INT_STATUS3_WATER)) {
			pdic_data->is_water_detect = true;
			sm5714_process_cc_water_det(pdic_data, WATER_MODE_ON);
	}
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (intr[1] & SM5714_REG_INT_STATUS2_SRC_ADV_CHG)
		sm5714_notify_rp_current_level(data);
#endif

	if (intr[1] & SM5714_REG_INT_STATUS2_VBUS_0V) {
#if defined(CONFIG_SM5714_SUPPORT_SBU)
		if (pdic_data->is_sbu_abnormal_state) {
			sm5714_pdic_event_work(pdic_data,
				PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
				PDIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
		}
#endif
		if (pdic_data->rid == RID_619K) {
			sm5714_pdic_event_work(pdic_data,
					PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_RID,
					pdic_data->rid/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
		}
	}

	if ((intr[0] & SM5714_REG_INT_STATUS1_VBUSPOK) &&
			(status[0] & SM5714_REG_INT_STATUS1_VBUSPOK)) {
		sm5714_check_cc_state(pdic_data);
		if (pdic_data->rid == RID_619K) {
			sm5714_pdic_event_work(pdic_data,
					PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_RID,
					pdic_data->rid/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
		}
	}

#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	if (intr[2] & SM5714_REG_INT_STATUS3_WATER_RLS) {
		if ((intr[2] & SM5714_REG_INT_STATUS3_WATER) == 0 &&
				(irq != (-1)) && pdic_data->is_water_detect && !is_lpcharge_pdic_param()) {
			pdic_data->is_water_detect = false;
			sm5714_process_cc_water_det(pdic_data, WATER_MODE_OFF);
		}
	}
#endif

	if ((intr[0] & SM5714_REG_INT_STATUS1_DETACH) &&
			(status[0] & SM5714_REG_INT_STATUS1_DETACH)) {
		pdic_data->status_reg |= PLUG_DETACH;
		sm5714_set_vconn_source(data, USBPD_VCONN_OFF);
		if (irq != (-1))
			sm5714_usbpd_set_vbus_dischg_gpio(pdic_data, 1);
	}

	mutex_lock(&pdic_data->lpm_mutex);
	if ((intr[0] & SM5714_REG_INT_STATUS1_ATTACH) &&
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
			(!pdic_data->is_water_detect) &&
#endif
			(status[0] & SM5714_REG_INT_STATUS1_ATTACH)) {
		pdic_data->status_reg |= PLUG_ATTACH;
		if (irq != (-1))
			sm5714_usbpd_set_vbus_dischg_gpio(pdic_data, 0);
	}
	mutex_unlock(&pdic_data->lpm_mutex);

	if (intr[3] & SM5714_REG_INT_STATUS4_HRST_RCVED) {
		pdic_data->status_reg |= MSG_HARDRESET;
		goto out;
	}

	if ((intr[1] & SM5714_REG_INT_STATUS2_PD_RID_DETECT))
		pdic_data->status_reg |= MSG_RID;

	/* JIG Case On */
	if (status[4] & SM5714_REG_INT_STATUS5_JIG_CASE_ON) {
		pdic_data->is_jig_case_on = true;
		goto out;
	}

	if ((intr[1] & SM5714_REG_INT_STATUS2_VCONN_DISCHG) ||
			(intr[2] & SM5714_REG_INT_STATUS3_VCONN_OCP))
		sm5714_set_vconn_source(data, USBPD_VCONN_OFF);

	if (intr[3] & SM5714_REG_INT_STATUS4_RX_DONE)
		sm5714_usbpd_protocol_rx(data);

	if (intr[3] & SM5714_REG_INT_STATUS4_TX_DONE) {
		data->protocol_tx.status = MESSAGE_SENT;
		pdic_data->status_reg |= MSG_GOODCRC;
	}

	if (intr[3] & SM5714_REG_INT_STATUS4_TX_DISCARD) {
		data->protocol_tx.status = TRANSMISSION_ERROR;
		pdic_data->status_reg |= MSG_PASS;
		sm5714_usbpd_tx_request_discard(data);
	}

	if (intr[3] & SM5714_REG_INT_STATUS4_TX_SOP_ERR)
		data->protocol_tx.status = TRANSMISSION_ERROR;

	if ((intr[3] & SM5714_REG_INT_STATUS4_PRL_RST_DONE) ||
			(intr[3] & SM5714_REG_INT_STATUS4_HCRST_DONE))
		pdic_data->reset_done = 1;

out:
	if (pdic_data->status_reg & data->wait_for_msg_arrived) {
		dev_info(pdic_data->dev, "%s: wait_for_msg_arrived = (%d)\n",
				__func__, data->wait_for_msg_arrived);
		data->wait_for_msg_arrived = 0;
		complete(&data->msg_arrived);
	}

	return 0;
}

static int sm5714_hard_reset(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret;
	u8 val;

	if (pdic_data->rid != REG_RID_UNDF &&
			pdic_data->rid != REG_RID_OPEN && pdic_data->rid != REG_RID_MAX)
		return 0;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL4, &val);
	val |= SM5714_REG_CNTL_HARD_RESET_MESSAGE;
	ret = sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL4, val);
	if (ret < 0)
		goto fail;

	pdic_data->status_reg = 0;

	return 0;

fail:
	return -EIO;
}

static int sm5714_receive_message(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct sm5714_policy_data *policy = &data->policy;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	int obj_num = 0;
	int ret = 0;
	u8 val;

	ret = sm5714_read_msg_header(i2c, &pdic_data->header);
	if (ret < 0)
		dev_err(dev, "%s read msg header error\n", __func__);

	obj_num = pdic_data->header.num_data_objs;

	if (obj_num > 0) {
		ret = sm5714_read_msg_obj(i2c,
			obj_num, &pdic_data->obj[0]);
	}

	sm5714_usbpd_read_reg(i2c, SM5714_REG_RX_SRC, &val);
	/* 0: SOP, 1: SOP', 2: SOP", 3: SOP' Debug, 4: SOP" Debug */
	policy->origin_message = val & 0x0F;
	dev_info(pdic_data->dev, "%s: Origin of Message = (%x)\n",
			__func__, val);

	/* Notify Rxed Message Read Done */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_RX_BUF, 0x80);

	return ret;
}

static int sm5714_tx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct sm5714_policy_data *policy = &data->policy;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret = 0;
	int count = 0;
	u8 tx_header1, tx_header2;

	mutex_lock(&pdic_data->_mutex);

	/* if there is no attach, skip tx msg */
	if (pdic_data->detach_valid)
		goto done;

	ret = sm5714_write_msg_header(i2c, header->byte);
	if (ret < 0)
		goto done;

	if (!pdic_data->pd_support && policy->state == PE_SNK_Select_Capability) {
		sm5714_usbpd_read_reg(i2c, SM5714_REG_TX_HEADER_00, &tx_header1);
		sm5714_usbpd_read_reg(i2c, SM5714_REG_TX_HEADER_01, &tx_header2);
		if ((tx_header1 != header->byte[0]) || (tx_header2 != header->byte[1]))
			pdic_data->soft_reset = true;
		dev_info(pdic_data->dev, "%s: TX_DATA[0x%x 0x%x] READ_DATA[0x%x 0x%x]\n",
				__func__, header->byte[0], header->byte[1], tx_header1, tx_header2);
	}
	count = header->num_data_objs;

	if (count > 0) {
		ret = sm5714_write_msg_obj(i2c, count, obj);
		if (ret < 0)
			goto done;
	}

	ret = sm5714_send_msg(data, i2c);
	if (ret < 0)
		goto done;

	pdic_data->status_reg = 0;
	data->wait_for_msg_arrived = 0;

done:
	mutex_unlock(&pdic_data->_mutex);
	return ret;
}

static int sm5714_rx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	int i;
	int count = 0;

	if (!sm5714_receive_message(data)) {
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

static void sm5714_get_short_state(void *_data, bool *val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;

#if defined(CONFIG_SM5714_SUPPORT_SBU)
	*val = (pdic_data->is_cc_abnormal_state ||
		pdic_data->is_sbu_abnormal_state);
#else
	*val = pdic_data->is_cc_abnormal_state;
#endif
}

static int sm5714_get_vconn_source(void *_data, int *val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;

	*val = pdic_data->vconn_source;
	return 0;
}

/* val : sink(0) or source(1) */
static int sm5714_set_power_role(void *_data, int val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	pr_info("%s: pr_swap received to %s\n",	__func__, val == 1 ? "SRC" : "SNK");

	if (val == USBPD_SINK) {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SNK;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
#endif
		sm5714_assert_rd(data);
		sm5714_set_snk(pdic_data->i2c);
	} else {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SOURCE;
#endif
		sm5714_assert_rp(data);
		sm5714_set_src(pdic_data->i2c);
	}

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (pdic_data->dual_role != NULL)
		dual_role_instance_changed(pdic_data->dual_role);
#elif defined(CONFIG_TYPEC)
	typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, val);
#endif
	pdic_data->power_role = val;
	return 0;
}

static int sm5714_get_power_role(void *_data, int *val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->power_role;
	return 0;
}

static int sm5714_set_data_role(void *_data, int val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct sm5714_usbpd_manager_data *manager;

	manager = &data->manager;
	if (!manager) {
		pr_err("%s : manager is null\n", __func__);
		return -ENODEV;
	}
	pr_info("%s: dr_swap received to %s\n", __func__, val == 1 ? "DFP" : "UFP");

	/* DATA_ROLE
	 * 0 : UFP
	 * 1 : DFP
	 */
	if (val == USBPD_UFP)
		sm5714_set_ufp(i2c);
	else
		sm5714_set_dfp(i2c);

	pdic_data->data_role = val;

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	/* exception code for 0x45 friends firmware */
	if (manager->dr_swap_cnt < INT_MAX)
		manager->dr_swap_cnt++;
	if (manager->Vendor_ID == SAMSUNG_VENDOR_ID &&
		manager->Product_ID == FRIENDS_PRODUCT_ID &&
		manager->dr_swap_cnt > 2) {
		pr_err("%s : skip %dth dr_swap message in samsung friends", __func__, manager->dr_swap_cnt);
		return -EPERM;
	}

	sm5714_process_dr_swap(pdic_data, val);
#endif
	return 0;
}

static int sm5714_get_data_role(void *_data, int *val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->data_role;
	return 0;
}

static int sm5714_set_check_msg_pass(void *_data, int val)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;

	dev_info(pdic_data->dev, "%s: check_msg_pass val(%d)\n", __func__, val);

	pdic_data->check_msg_pass = val;

	return 0;
}

void sm5714_set_bist_carrier_m2(void *_data)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL2, &val);
	val |= 0x10;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, val);

	msleep(30);

	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL2, &val);
	val &= 0xEF;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, val);
	pr_info("%s\n", __func__);
}

void sm5714_error_recovery_mode(void *_data)
{
	struct sm5714_usbpd_data *pd_data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int power_role = 0;
	u8 data;

	sm5714_get_power_role(pd_data, &power_role);

	if (pdic_data->is_attached) {
		/* SRC to SNK pr_swap fail case when counter part is MPSM mode */
		if (power_role == USBPD_SINK) {
			sm5714_set_attach(pdic_data, TYPE_C_ATTACH_DFP);
		} else {
			/* SRC to SRC when hard reset occurred 2times */
			if (pd_data->counter.hard_reset_counter > USBPD_nHardResetCount)
				sm5714_set_attach(pdic_data, TYPE_C_ATTACH_DFP);
			/* SNK to SRC pr_swap fail case */
			else {
				/* SNK_Only */
				sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL1, 0x45);
				sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL3, &data);
				data |= 0x04; /* go to ErrorRecovery State */
				sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, data);
			}
		}
	}
	pr_info("%s: power_role = %s\n", __func__, power_role ? "SRC" : "SNK");
}

void sm5714_mpsm_enter_mode_change(struct sm5714_phydrv_data *usbpd_data)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(usbpd_data->dev);
	int power_role = 0;

	sm5714_get_power_role(pd_data, &power_role);
	switch (power_role) {
	case PDIC_SINK: /* SNK */
		pr_info("%s : do nothing for SNK\n", __func__);
		break;
	case PDIC_SOURCE: /* SRC */
		sm5714_usbpd_kick_policy_work(usbpd_data->dev);
		break;
	};
}

void sm5714_mpsm_exit_mode_change(struct sm5714_phydrv_data *usbpd_data)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(usbpd_data->dev);
	int power_role = 0;

	sm5714_get_power_role(pd_data, &power_role);
	switch (power_role) {
	case PDIC_SINK: /* SNK */
		pr_info("%s : do nothing for SNK\n", __func__);
		break;
	case PDIC_SOURCE: /* SRC */
		pr_info("%s : reattach to SRC\n", __func__);
		usbpd_data->is_mpsm_exit = 1;
		reinit_completion(&usbpd_data->exit_mpsm_completion);
		sm5714_set_detach(usbpd_data, TYPE_C_ATTACH_DFP);
		msleep(400); /* debounce time */
		sm5714_set_attach(usbpd_data, TYPE_C_ATTACH_DFP);
		if (!wait_for_completion_timeout(
				&usbpd_data->exit_mpsm_completion,
				msecs_to_jiffies(400))) {
			pr_err("%s: SRC reattach failed\n", __func__);
			usbpd_data->is_mpsm_exit = 0;
			sm5714_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
		} else {
			pr_err("%s: SRC reattach success\n", __func__);
		}
		break;
	};
}

void sm5714_set_enable_pd_function(void *_data, int enable)
{
	struct sm5714_usbpd_data *data = (struct sm5714_usbpd_data *) _data;
	struct sm5714_phydrv_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int power_role = 0;

	if (enable && pdic_data->is_jig_case_on) {
		pr_info("%s: Do not enable pd function.\n", __func__);
		return;
	}

	sm5714_get_power_role(data, &power_role);

	pr_info("%s: enable : (%d), power_role : (%s)\n", __func__,
		enable,	power_role ? "SOURCE" : "SINK");

	if (enable == PD_ENABLE)
		sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL1, 0x08);
	else
		sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL1, 0x00);
}

static void sm5714_notify_pdic_rid(struct sm5714_phydrv_data *pdic_data,
	int rid)
{
	u8 reg_data = 0;

	pdic_data->is_factory_mode = false;

#if defined(CONFIG_SM5714_FACTORY_MODE) && IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_CHARGER_SM5714)
	if (rid == RID_301K || rid == RID_523K || rid == RID_619K) {
		if (rid == RID_523K)
			pdic_data->is_factory_mode = true;
		sm5714_charger_oper_en_factory_mode(DEV_TYPE_SM5714_PDIC,
				rid, 1);
	} else if (rid == RID_OPEN) {
		sm5714_charger_oper_en_factory_mode(DEV_TYPE_SM5714_PDIC,
				rid, 0);
	}
#else
	if (rid == RID_523K)
		pdic_data->is_factory_mode = true;
#endif
	/* rid */
	sm5714_pdic_event_work(pdic_data,
		PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_RID,
		rid/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);

	if (rid == REG_RID_523K || rid == REG_RID_619K || rid == REG_RID_OPEN)
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
#endif
		sm5714_pdic_event_work(pdic_data,
			PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
			PDIC_NOTIFY_DETACH/*attach*/,
			USB_STATUS_NOTIFY_DETACH, 0);
	msleep(300);
	sm5714_usbpd_read_reg(pdic_data->i2c, SM5714_REG_JIGON_CONTROL, &reg_data);
	reg_data &= 0xFD;
	sm5714_usbpd_write_reg(pdic_data->i2c, SM5714_REG_JIGON_CONTROL, reg_data);
}

static void sm5714_usbpd_check_rid(struct sm5714_phydrv_data *pdic_data)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 rid;
	int prev_rid = pdic_data->rid;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_FACTORY, &rid);

	dev_info(pdic_data->dev, "%s : attached rid state(%d)", __func__, rid);

	sm5714_set_enable_pd_function(pd_data, PD_DISABLE);
#if defined(CONFIG_SEC_FACTORY)
	sm5714_factory_execute_monitor(pdic_data, FAC_ABNORMAL_REPEAT_RID);
#endif

	if (rid) {
		if (prev_rid != rid) {
			pdic_data->rid = rid;
			if (prev_rid >= REG_RID_OPEN && rid >= REG_RID_OPEN)
				dev_err(pdic_data->dev,
				"%s : rid is not changed, skip notify(%d)",
				__func__, rid);
			else
				sm5714_notify_pdic_rid(pdic_data, rid);
		}

		if (rid >= REG_RID_MAX) {
			dev_err(pdic_data->dev, "%s : overflow rid value",
					__func__);
			return;
		}
	}
}

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
struct usbpd_ops ops_usbpd = {
	.usbpd_sbu_test_read = sm5714_usbpd_sbu_test_read,
	.usbpd_set_host_on = sm5714_usbpd_set_host_on,
};
#endif

static int check_usb_killer(struct sm5714_phydrv_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0;
	int ret = 0, retry = 0;

	sm5714_usbpd_write_reg(i2c, SM5714_REG_USBK_CNTL, 0x80);
	msleep(5);

	for (retry = 0; retry < 3; retry++) {
		sm5714_usbpd_read_reg(i2c, SM5714_REG_USBK_CNTL, &reg_data);
		pr_info("%s, USBK_CNTL : 0x%x\n", __func__, reg_data);
		if (reg_data & 0x02) {
			if (reg_data & 0x01)
				ret = 1;
			else
				ret = 0;
			break;
		} else {
			pr_info("%s, DPDM_DONE is not yet, retry : %d\n", __func__, retry+1);
		}
	}

	return ret;
}

void sm5714_vbus_turn_on_ctrl(struct sm5714_phydrv_data *usbpd_data,
	bool enable)
{
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct power_supply *psy_otg;
	union power_supply_propval val;
#endif
	int on = !!enable;
	int ret = 0;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(usbpd_data->dev);
	struct sm5714_policy_data *policy = &pd_data->policy;
	struct otg_notify *o_notify = get_otg_notify();
	bool must_block_host = is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST);
	static int reserve_booster = 0;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	pr_info("%s : enable=%d, must_block_host=%d\n",
		__func__, enable, must_block_host);
	if (must_block_host) {
		pr_info("%s : turn off vbus because of blocked host\n",	__func__);
		return;
	}
	if (enable && (policy->state != PE_PRS_SNK_SRC_Source_on) &&
			(policy->state != PE_SRC_Transition_to_default) &&
			check_usb_killer(usbpd_data)) {
		pr_info("%s : do not turn on VBUS because of USB Killer.\n",
			__func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_USBKILLER;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_USB_KILLER_COUNT);
#endif
		return;
	}
#endif
	pr_info("%s : enable=%d\n", __func__, enable);
	
#if defined(CONFIG_USB_HOST_NOTIFY)	
	if (o_notify && o_notify->booting_delay_sec && enable) {
		pr_info("%s %d, is booting_delay_sec. skip to control booster\n",
			__func__, __LINE__);
		reserve_booster = 1;
		send_otg_notify(o_notify, NOTIFY_EVENT_RESERVE_BOOSTER, 1);
		return;
	}
	if (!enable) {
		if (reserve_booster) {
			reserve_booster = 0;
			send_otg_notify(o_notify, NOTIFY_EVENT_RESERVE_BOOSTER, 0);
		}
	}
#endif
	
	
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_otg = get_power_supply_by_name("otg");

	if (psy_otg) {
		val.intval = enable;
		usbpd_data->is_otg_vboost = enable;
		ret = psy_otg->desc->set_property(psy_otg,
				POWER_SUPPLY_PROP_ONLINE, &val);
	} else {
		pr_err("%s: Fail to get psy battery\n", __func__);
	}
#endif
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	} else {
		pr_info("otg accessory power = %d\n", on);
	}
}

static int sm5714_usbpd_notify_attach(void *data)
{
	struct sm5714_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	bool short_cable = false;
#endif
	u8 reg_data;
	int ret = 0;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	int prev_power_role = pdic_data->power_role_dual;
#elif defined(CONFIG_TYPEC)
	int prev_power_role = pdic_data->typec_power_role;
#endif

	ret = sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_STATUS, &reg_data);
	if (ret < 0)
		dev_err(dev, "%s, i2c read CC_STATUS error\n", __func__);
	pdic_data->is_attached = 1;
	pdic_data->abnormal_dev_cnt = 0;
	/* cc_SINK */
	if ((reg_data & SM5714_ATTACH_TYPE) == SM5714_ATTACH_SOURCE) {
		dev_info(dev, "ccstat : cc_SINK\n");
		manager->pn_flag = false;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		if (prev_power_role == DUAL_ROLE_PROP_PR_SRC)
#elif defined(CONFIG_TYPEC)
		if (prev_power_role == TYPEC_SOURCE)
#endif
			sm5714_vbus_turn_on_ctrl(pdic_data, 0);
#if defined(CONFIG_SEC_FACTORY)
		sm5714_factory_execute_monitor(pdic_data,
				FAC_ABNORMAL_REPEAT_STATE);
#endif
#if defined(CONFIG_SM5714_SUPPORT_SBU)
		sm5714_short_state_check(pdic_data);
#endif

#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
		if (pdic_data->is_water_detect)
			return -1;
#endif

		pdic_data->power_role = PDIC_SINK;
		pdic_data->data_role = USBPD_UFP;
		sm5714_set_snk(i2c);
		sm5714_set_ufp(i2c);

		if (pdic_data->is_factory_mode == true)
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		{
			/* muic */
			sm5714_pdic_event_work(pdic_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
			PDIC_NOTIFY_ATTACH/*attach*/,
			USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
			return true;
		}
#else
		return true;
#endif
		sm5714_usbpd_policy_reset(pd_data, PLUG_EVENT);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		sm5714_get_short_state(pd_data, &short_cable);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		/* muic, battery */
		if (short_cable) {
			/* rp abnormal */
			sm5714_notify_rp_abnormal(pd_data);
		} else {
			/* rp current */
			sm5714_notify_rp_current_level(pd_data);
		}
#endif
		if (!(pdic_data->rid == REG_RID_523K ||
				pdic_data->rid == REG_RID_619K)) {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
			pdic_data->power_role_dual =
					DUAL_ROLE_PROP_PR_SNK;
			if (pdic_data->dual_role != NULL &&
				pdic_data->data_role != USB_STATUS_NOTIFY_DETACH)
				dual_role_instance_changed(pdic_data->dual_role);
#elif defined(CONFIG_TYPEC)
			if (!pdic_data->detach_valid &&
				pdic_data->typec_data_role == TYPEC_HOST) {
				sm5714_pdic_event_work(pdic_data,
					PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
					PDIC_NOTIFY_DETACH/*attach*/,
					USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
				dev_info(dev, "directly called from DFP to UFP\n");
			}
			pdic_data->typec_power_role = TYPEC_SINK;
			typec_set_pwr_role(pdic_data->port, TYPEC_SINK);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
			if (o_notify)
				send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
			sm5714_pdic_event_work(pdic_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				PDIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
		}
#endif
		sm5714_set_vconn_source(pd_data, USBPD_VCONN_OFF);
	/* cc_SOURCE */
	} else if (((reg_data & SM5714_ATTACH_TYPE) == SM5714_ATTACH_SINK) &&
			check_usb_killer(pdic_data) == 0) {
		dev_info(dev, "ccstat : cc_SOURCE\n");
		if (pdic_data->is_mpsm_exit) {
			complete(&pdic_data->exit_mpsm_completion);
			pdic_data->is_mpsm_exit = 0;
			dev_info(dev, "exit mpsm completion\n");
		}
#ifndef CONFIG_SEC_FACTORY
		if (pdic_data->scr_sel == PLUG_CTRL_RP180)
			sm5714_usbpd_set_rp_scr_sel(pd_data, PLUG_CTRL_RP80);
#endif
		sm5714_set_vconn_source(pd_data, USBPD_VCONN_ON);
		manager->pn_flag = false;
		/* add to turn on external 5V */
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		if (prev_power_role == DUAL_ROLE_PROP_PR_NONE)
#elif defined(CONFIG_TYPEC)
		if (prev_power_role == TYPEC_SINK)
#endif
		sm5714_vbus_turn_on_ctrl(pdic_data, 1);
		pdic_data->power_role = PDIC_SOURCE;
		pdic_data->data_role = USBPD_DFP;

		sm5714_set_enable_pd_function(pd_data, PD_ENABLE);
		sm5714_usbpd_policy_reset(pd_data, PLUG_EVENT);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		/* muic */
		sm5714_pdic_event_work(pdic_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
			PDIC_NOTIFY_ATTACH/*attach*/,
			USB_STATUS_NOTIFY_ATTACH_DFP/*rprd*/, 0);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
		if (pdic_data->dual_role != NULL &&
			pdic_data->data_role != USB_STATUS_NOTIFY_DETACH)
			dual_role_instance_changed(pdic_data->dual_role);
#elif defined(CONFIG_TYPEC)
		if (!pdic_data->detach_valid &&
			pdic_data->typec_data_role == TYPEC_DEVICE) {
			sm5714_pdic_event_work(pdic_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				PDIC_NOTIFY_DETACH/*attach*/,
				USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
			dev_info(dev, "directly called from UFP to DFP\n");
		}
		pdic_data->typec_power_role = TYPEC_SOURCE;
		typec_set_pwr_role(pdic_data->port, TYPEC_SOURCE);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		if (o_notify)
			send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 1);
#endif
		/* USB */
		sm5714_pdic_event_work(pdic_data,
				PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
				PDIC_NOTIFY_ATTACH/*attach*/,
				USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/, 0);
#endif /* CONFIG_PDIC_NOTIFIER */
		sm5714_set_dfp(i2c);
		sm5714_set_src(i2c);
		msleep(180); /* don't over 310~620ms(tTypeCSinkWaitCap) */
		/* cc_AUDIO */
	} else if ((reg_data & SM5714_ATTACH_TYPE) == SM5714_ATTACH_AUDIO) {
#ifndef CONFIG_SEC_FACTORY
		if (sm5714_usbpd_check_normal_audio_device(pdic_data) == 0) {
			/* Set 'CC_UNATT_SRC' state */
			sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, 0x81);
			return -1;
		}
#endif
		dev_info(dev, "ccstat : cc_AUDIO\n");
		manager->acc_type = PDIC_DOCK_UNSUPPORTED_AUDIO;
		sm5714_usbpd_check_accessory(manager);
	} else {
		dev_err(dev, "%s, PLUG Error\n", __func__);
		return -1;
	}

	pdic_data->detach_valid = false;

	return ret;
}

static void sm5714_usbpd_notify_detach(void *data)
{
	struct sm5714_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5714_usbpd_manager_data *manager = &pd_data->manager;
	u8 reg_data = 0;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	dev_info(dev, "ccstat : cc_No_Connection\n");
	sm5714_vbus_turn_on_ctrl(pdic_data, 0);
	pdic_data->is_attached = 0;
	pdic_data->status_reg = 0;
	sm5714_usbpd_reinit(dev);

	sm5714_usbpd_read_reg(i2c, SM5714_REG_JIGON_CONTROL, &reg_data);
	reg_data |= 0x02;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_JIGON_CONTROL, reg_data);

#if defined(CONFIG_SM5714_FACTORY_MODE) && IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_CHARGER_SM5714)
	if ((pdic_data->rid == REG_RID_301K) ||
			(pdic_data->rid == REG_RID_523K) ||
			(pdic_data->rid == REG_RID_619K))
		sm5714_charger_oper_en_factory_mode(DEV_TYPE_SM5714_PDIC,
				REG_RID_OPEN, 0);
#endif
	pdic_data->rid = REG_RID_MAX;
	pd_data->pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	pd_data->pd_noti.sink_status.current_pdo_num = 0;
	pd_data->pd_noti.sink_status.selected_pdo_num = 0;
	pd_data->pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
	pd_data->pd_noti.sink_status.pps_voltage = 0;
	pd_data->pd_noti.sink_status.pps_current = 0;
	pd_data->pd_noti.sink_status.has_apdo = false;
	/* BATTERY */
	sm5714_pdic_event_work(pdic_data,
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
		PDIC_NOTIFY_DEV_BATTERY,
#else
		PDIC_NOTIFY_DEV_BATT,
#endif
		PDIC_NOTIFY_ID_POWER_STATUS,
		PDIC_NOTIFY_DETACH/*attach*/,
		USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);

	sm5714_set_vconn_source(pd_data, USBPD_VCONN_OFF);
	pdic_data->detach_done_wait = 1;
	pdic_data->detach_valid = true;
	pdic_data->is_factory_mode = false;
	pdic_data->is_cc_abnormal_state = false;
#if defined(CONFIG_SM5714_SUPPORT_SBU)
	pdic_data->is_sbu_abnormal_state = false;
#endif
	pdic_data->is_jig_case_on = false;
	pdic_data->reset_done = 0;
	pdic_data->pd_support = 0;
	pdic_data->rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	sm5714_usbpd_policy_reset(pd_data, PLUG_DETACHED);
#if defined(CONFIG_TYPEC)
	if (pdic_data->partner) {
		pr_info("%s : typec_unregister_partner - pd_support : %d\n",
			__func__, pdic_data->pd_support);
		if (!IS_ERR(pdic_data->partner))
			typec_unregister_partner(pdic_data->partner);
		pdic_data->partner = NULL;
		pdic_data->typec_power_role = TYPEC_SINK;
		pdic_data->typec_data_role = TYPEC_DEVICE;
		pdic_data->pwr_opmode = TYPEC_PWR_MODE_USB;
	}
	if (pdic_data->typec_try_state_change == TRY_ROLE_SWAP_PR ||
		pdic_data->typec_try_state_change == TRY_ROLE_SWAP_DR) {
		/* Role change try and new mode detected */
		pr_info("%s : typec_reverse_completion, detached while pd/dr_swap",
			__func__);
		pdic_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		complete(&pdic_data->typec_reverse_completion);
	}
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify) {
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
		send_otg_notify(o_notify, NOTIFY_EVENT_DR_SWAP, 0);
	}
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	/* MUIC */
	sm5714_pdic_event_work(pdic_data,
		PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
		PDIC_NOTIFY_DETACH/*attach*/,
		USB_STATUS_NOTIFY_DETACH/*rprd*/, 0);
	sm5714_pdic_event_work(pdic_data,
		PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_RID,
		REG_RID_OPEN/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
		/* usb or otg */
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
	pdic_data->typec_power_role = TYPEC_SINK;
#endif
	/* USB */
	sm5714_pdic_event_work(pdic_data,
		PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
		PDIC_NOTIFY_DETACH/*attach*/,
		USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (!pdic_data->try_state_change && !(pdic_data->scr_sel == PLUG_CTRL_RP180))
#elif defined(CONFIG_TYPEC)
	if (!pdic_data->typec_try_state_change && !(pdic_data->scr_sel == PLUG_CTRL_RP180))
#endif
		sm5714_rprd_mode_change(pdic_data, TYPE_C_ATTACH_DRP);
#endif /* end of CONFIG_PDIC_NOTIFIER */

	/* release SBU sourcing for water detection */
	sm5714_usbpd_write_reg(i2c, 0x24, 0x00);

	/* Set Sink / UFP */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL2, 0x04);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE3, &reg_data);
	if (reg_data & 0x06) /* Reset Done */
		sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL4, 0x01);
	else /* Protocol Layer reset */
		sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL4, 0x00);
	sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL1, 0x00);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL7, &reg_data);
	reg_data &= 0xFC;
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL7, reg_data);
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, 0x80);
	sm5714_usbpd_acc_detach(dev);
	if (manager->dp_is_connect == 1)
		sm5714_usbpd_dp_detach(dev);
	manager->dr_swap_cnt = 0;

	if (pdic_data->soft_reset) {
		pdic_data->soft_reset = false;
		dev_info(dev, "Do soft reset.\n");
		sm5714_usbpd_write_reg(i2c, SM5714_REG_SYS_CNTL, 0x80);
		sm5714_driver_reset(pd_data);
		sm5714_usbpd_reg_init(pdic_data);
	}
}

/* check RID again for attached cable case */
bool sm5714_second_check_rid(struct sm5714_phydrv_data *data)
{
	struct sm5714_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);

	if (sm5714_get_status(pd_data, PLUG_ATTACH)) {
		if (sm5714_usbpd_notify_attach(data) < 0)
			return false;
	}

	if (sm5714_get_status(pd_data, MSG_RID))
		sm5714_usbpd_check_rid(pdic_data);
	return true;
}

static irqreturn_t sm5714_pdic_irq_thread(int irq, void *data)
{
	struct sm5714_phydrv_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(dev);
	int ret = 0;
	unsigned int rid_status = 0;
#if defined(CONFIG_SEC_FACTORY)
	u8 rid;
#endif /* CONFIG_SEC_FACTORY */

	dev_info(dev, "%s, irq = %d\n", __func__, irq);

#if defined(CONFIG_ARCH_QCOM_NO) || defined(CONFIG_ARCH_MEDIATEK)
	__pm_stay_awake(pdic_data->irq_ws);
	ret = wait_event_timeout(pdic_data->suspend_wait,
						!pdic_data->suspended,
						msecs_to_jiffies(200));
	if (!ret) {
		pr_info("%s suspend_wait timeout\n", __func__);
		__pm_relax(pdic_data->irq_ws);
		return IRQ_HANDLED;
	}
#endif

	mutex_lock(&pdic_data->_mutex);

	sm5714_poll_status(pd_data, irq);

	if (sm5714_get_status(pd_data, MSG_NONE))
		goto out;

#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	if (pdic_data->is_water_detect)
		goto out;
#endif

	if (sm5714_get_status(pd_data, MSG_HARDRESET)) {
		sm5714_usbpd_rx_hard_reset(dev);
		sm5714_usbpd_kick_policy_work(dev);
		goto out;
	}

	if (sm5714_get_status(pd_data, MSG_SOFTRESET)) {
		sm5714_usbpd_rx_soft_reset(pd_data);
		sm5714_usbpd_kick_policy_work(dev);
		goto out;
	}

	if (sm5714_get_status(pd_data, PLUG_ATTACH)) {
		pr_info("%s PLUG_ATTACHED +++\n", __func__);
		rid_status = sm5714_get_status(pd_data, MSG_RID);
		ret = sm5714_usbpd_notify_attach(pdic_data);
		if (ret >= 0) {
			if (rid_status)
				sm5714_usbpd_check_rid(pdic_data);
			goto hard_reset;
		}
	}

	if (sm5714_get_status(pd_data, PLUG_DETACH)) {
		pr_info("%s PLUG_DETACHED ---\n", __func__);
#if defined(CONFIG_SEC_FACTORY)
			sm5714_factory_execute_monitor(pdic_data,
					FAC_ABNORMAL_REPEAT_STATE);
			if (pdic_data->rid == REG_RID_619K) {
				msleep(250);
				sm5714_usbpd_read_reg(i2c, SM5714_REG_FACTORY, &rid);
				pr_info("%s : Detached, check if still 619K? => 0x%X\n",
						__func__, rid);
				if (rid == REG_RID_619K) {
					if (!sm5714_second_check_rid(pdic_data))
						goto out;
					else
						goto hard_reset;
				}
			}
#else
			if (pdic_data->is_otg_vboost) {
				dev_info(&i2c->dev, "%s : Detached, go back to 80uA\n",
					__func__);
				sm5714_usbpd_set_rp_scr_sel(pd_data, PLUG_CTRL_RP80);
			}
#endif /* CONFIG_SEC_FACTORY */
		sm5714_usbpd_notify_detach(pdic_data);
		goto out;
	}

	if (!sm5714_second_check_rid(pdic_data))
		goto out;

hard_reset:
	mutex_lock(&pdic_data->lpm_mutex);
	sm5714_usbpd_kick_policy_work(dev);
	mutex_unlock(&pdic_data->lpm_mutex);
out:

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER_SM5714)
	schedule_delayed_work(&pdic_data->vbus_noti_work, 0);
#endif /* CONFIG_VBUS_NOTIFIER */

	mutex_unlock(&pdic_data->_mutex);
#if defined(CONFIG_ARCH_QCOM_NO) || defined(CONFIG_ARCH_MEDIATEK)
	__pm_relax(pdic_data->irq_ws);
#endif

	return IRQ_HANDLED;
}

static int sm5714_usbpd_reg_init(struct sm5714_phydrv_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	u8 reg_data;

	pr_info("%s", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (!lpcharge)	/* Release SNK Only */
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL1, 0x41);
#endif
	sm5714_check_cc_state(_data);
	/* Release SBU Sourcing */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL5, 0x00);
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	sm5714_abnormal_dev_int_on_off(_data, 1);
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL4, 0x03);
	/* Enable Rp Attach even under Water Status */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_OPT4, 0x7B);
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL9, 0x10);
	/* Water Detection CC & SBU Threshold 1Mohm */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_TH3, 0x29);
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_TH7, 0x29);
	/* Water Release CC & SBU Threshold 1.2Mohm */
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_TH6, 0x13);
#else
	sm5714_usbpd_write_reg(i2c, SM5714_REG_CORR_CNTL4, 0x00);
#endif
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE3, &reg_data);
	if (reg_data & 0x06) /* Reset Wait Policy Engine */
		sm5714_usbpd_write_reg(i2c, SM5714_REG_PD_CNTL4,
				SM5714_REG_CNTL_NOTIFY_RESET_DONE);

	return 0;
}

static int sm5714_usbpd_irq_init(struct sm5714_phydrv_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;
	int ret = 0;

	pr_info("%s", __func__);
	if (!_data->irq_gpio) {
		dev_err(dev, "%s No interrupt specified\n", __func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(_data->irq_gpio);
	
	ret = gpio_request(_data->irq_gpio, "usbpd_irq");
	if (ret) {
		dev_err(_data->dev, "%s: failed requesting gpio %d\n",
			__func__, _data->irq_gpio);
		return ret;
	}
	gpio_direction_input(_data->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
			sm5714_pdic_irq_thread,
			(IRQF_TRIGGER_LOW | IRQF_ONESHOT),
			"sm5714-usbpd", _data);
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
	sm5714_set_irq_enable(_data, ENABLED_INT_1, ENABLED_INT_2,
			ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	return ret;
}

#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
static void sm5714_power_off_water_check(struct sm5714_phydrv_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	u8 adc_sbu1, adc_sbu2, adc_sbu3, adc_sbu4, status2, status3;

	sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS2, &status2);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_STATUS3, &status3);
	pr_info("%s, STATUS2 = 0x%x, STATUS3 = 0x%x\n", __func__, status2, status3);

	if (status3 & SM5714_REG_INT_STATUS3_WATER ||
			status2 & SM5714_REG_INT_STATUS2_VBUS_0V)
		return;

	sm5714_corr_sbu_volt_read(_data, &adc_sbu1, &adc_sbu2, SBU_SOURCING_OFF);
	if (adc_sbu1 > 0xA && adc_sbu2 > 0xA) {
		_data->is_water_detect = true;
		sm5714_process_cc_water_det(_data, WATER_MODE_ON);
		pr_info("%s, TA with water.\n", __func__);
		return;
	} else if (adc_sbu1 > 0x3E || adc_sbu2 > 0x3E) {
		sm5714_corr_sbu_volt_read(_data, &adc_sbu3, &adc_sbu4, SBU_SOURCING_ON);
		if ((adc_sbu1 < 0x2 || adc_sbu2 < 0x2) &&
			(adc_sbu3 > 0x3E || adc_sbu4 > 0x3E)) {
			return;
		}
		_data->is_water_detect = true;
		sm5714_process_cc_water_det(_data, WATER_MODE_ON);
		pr_info("%s, TA with water.\n", __func__);
		return;
	}
}
#endif

static int of_sm5714_pdic_dt(struct device *dev,
			struct sm5714_phydrv_data *_data)
{
	struct device_node *np_usbpd = dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);
	if (np_usbpd == NULL) {
		dev_err(dev, "%s np NULL\n", __func__);
		ret = -EINVAL;
	} else {
		_data->irq_gpio = of_get_named_gpio(np_usbpd,
							"usbpd,usbpd_int", 0);
		if (_data->irq_gpio < 0) {
			dev_err(dev, "error reading usbpd irq = %d\n",
						_data->irq_gpio);
			_data->irq_gpio = 0;
		}
		pr_info("%s irq_gpio = %d", __func__, _data->irq_gpio);

		_data->vbus_dischg_gpio = of_get_named_gpio(np_usbpd,
							"usbpd,vbus_discharging", 0);
		if (gpio_is_valid(_data->vbus_dischg_gpio))
			pr_info("%s vbus_discharging = %d\n",
						__func__, _data->vbus_dischg_gpio);

		if (of_find_property(np_usbpd, "vconn-en", NULL))
			_data->vconn_en = true;
		else
			_data->vconn_en = false;
	}

	return ret;
}

void sm5714_manual_JIGON(struct sm5714_phydrv_data *usbpd_data, int mode)
{
	struct i2c_client *i2c = usbpd_data->i2c;

	pr_info("usb: mode=%s", mode ? "High" : "Low");
	if (mode == 1)
		sm5714_usbpd_write_reg(i2c, SM5714_REG_JIGON_CONTROL, 0x03);
	else
		sm5714_usbpd_write_reg(i2c, SM5714_REG_JIGON_CONTROL, 0x00);
}

static int sm5714_handle_usb_external_notifier_notification(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct sm5714_phydrv_data *usbpd_data = container_of(nb,
		struct sm5714_phydrv_data, usb_external_notifier_nb);
	int ret = 0;
	int enable = *(int *)data;

	pr_info("%s : action=%lu , enable=%d\n", __func__, action, enable);
	switch (action) {
	case EXTERNAL_NOTIFY_HOSTBLOCK_PRE:
		if (enable) {
			pr_info("%s : EXTERNAL_NOTIFY_HOSTBLOCK_PRE\n", __func__);
			/* sm5714_set_enable_alternate_mode(ALTERNATE_MODE_STOP); */
			sm5714_mpsm_enter_mode_change(usbpd_data);
		} else {
		}
		break;
	case EXTERNAL_NOTIFY_HOSTBLOCK_POST:
		if (enable) {
		} else {
			pr_info("%s : EXTERNAL_NOTIFY_HOSTBLOCK_POST\n", __func__);
			/* sm5714_set_enable_alternate_mode(ALTERNATE_MODE_START); */
			sm5714_mpsm_exit_mode_change(usbpd_data);
		}
		break;
	}

	return ret;
}

static void sm5714_delayed_external_notifier_init(struct work_struct *work)
{
	int ret = 0;
	static int retry_count = 1;
	int max_retry_count = 5;
	struct delayed_work *delay_work =
		container_of(work, struct delayed_work, work);
	struct sm5714_phydrv_data *usbpd_data =
		container_of(delay_work,
			struct sm5714_phydrv_data, usb_external_notifier_register_work);

	pr_info("%s : %d = times!\n", __func__, retry_count);

	/* Register pdic handler to pdic notifier block list */
	ret = usb_external_notify_register(&usbpd_data->usb_external_notifier_nb,
		sm5714_handle_usb_external_notifier_notification,
		EXTERNAL_NOTIFY_DEV_PDIC);
	if (ret < 0) {
		pr_err("Manager notifier init time is %d.\n", retry_count);
		if (retry_count++ != max_retry_count)
			schedule_delayed_work(
			&usbpd_data->usb_external_notifier_register_work,
			msecs_to_jiffies(2000));
		else
			pr_err("fail to init external notifier\n");
	} else
		pr_info("%s : external notifier register done!\n", __func__);
}

static void sm5714_usbpd_debug_reg_log(struct work_struct *work)
{
	struct sm5714_phydrv_data *pdic_data =
		container_of(work, struct sm5714_phydrv_data,
				debug_work.work);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data[20] = {0, };

	sm5714_usbpd_read_reg(i2c, SM5714_REG_JIGON_CONTROL, &data[0]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CORR_CNTL1, &data[1]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CORR_CNTL4, &data[2]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CORR_CNTL5, &data[3]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_STATUS, &data[4]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL1, &data[5]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL2, &data[6]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL3, &data[7]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL7, &data[8]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL1, &data[9]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_CNTL4, &data[10]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_RX_BUF_ST, &data[11]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PROBE0, &data[12]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE0, &data[13]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE2, &data[14]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE3, &data[15]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE4, &data[16]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_PD_STATE5, &data[17]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_COMP_CNTL, &data[18]);
	sm5714_usbpd_read_reg(i2c, SM5714_REG_CLK_CNTL, &data[19]);

	pr_info("%s JIGON:0x%02x CR_CT[1: 0x%02x 4:0x%02x 5:0x%02x] CC_ST:0x%02x CC_CT[1:0x%02x 2:0x%02x 3:0x%02x 7:0x%02x] PD_CT[1:0x%02x 4:0x%02x] RX_BUF_ST:0x%02x PROBE0:0x%02x PD_ST[0:0x%02x 2:0x%02x 3:0x%02x 4:0x%02x 5:0x%02x]COMP:0x%02x CLK:0x%02x\n",
			__func__, data[0], data[1], data[2], data[3], data[4],
			data[5], data[6], data[7], data[8], data[9], data[10],
			data[11], data[12], data[13], data[14], data[15],
			data[16], data[17],data[18], data[19]);

	if (!pdic_data->suspended)
		schedule_delayed_work(&pdic_data->debug_work,
				msecs_to_jiffies(60000));
}

static int sm5714_usbpd_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct sm5714_phydrv_data *pdic_data;
	struct device *dev = &i2c->dev;
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	struct usbpd_dev *usbpd_d;
#endif
	int ret = 0;
	u8 rid = 0;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	ppdic_data_t ppdic_data;
	ppdic_sysfs_property_t ppdic_sysfs_prop;
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	u8 reg_data = 0;
#endif

	pr_info("%s start\n", __func__);
	test_i2c = i2c;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	pdic_data = kzalloc(sizeof(struct sm5714_phydrv_data), GFP_KERNEL);
	if (!pdic_data) {
		dev_err(dev, "%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	pdic_data->dev = &i2c->dev;
	pdic_data->i2c = i2c;

	ret = of_sm5714_pdic_dt(&i2c->dev, pdic_data);
	if (ret < 0)
		dev_err(dev, "%s: not found dt!\n", __func__);

	sm5714_usbpd_read_reg(i2c, SM5714_REG_FACTORY, &rid);

	pdic_data->rid = rid;
	pdic_data->lpm_mode = false;
	pdic_data->is_factory_mode = false;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (factory_mode) {
		if (rid != REG_RID_523K) {
			dev_err(dev, "%s : In factory mode, but RID is not 523K, RID : %x\n",
					__func__, rid);
		} else {
			dev_err(dev, "%s : In factory mode, but RID is 523K OK\n",
					__func__);
			pdic_data->is_factory_mode = true;
		}
	} else {
		if (rid == REG_RID_OPEN) {
			sm5714_usbpd_read_reg(i2c, SM5714_REG_JIGON_CONTROL, &reg_data);
			reg_data |= 0x02;
			sm5714_usbpd_write_reg(i2c, SM5714_REG_JIGON_CONTROL, reg_data);
		}
	}
#endif
	pdic_data->check_msg_pass = false;
	pdic_data->vconn_source = USBPD_VCONN_OFF;
	pdic_data->rid = REG_RID_MAX;
	pdic_data->is_attached = 0;
#if defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	pdic_data->is_water_detect = false;
#endif
	pdic_data->detach_valid = true;
	pdic_data->is_otg_vboost = false;
	pdic_data->is_jig_case_on = false;
	pdic_data->soft_reset = false;
	pdic_data->is_timer_expired = false;
	pdic_data->reset_done = 0;
	pdic_data->abnormal_dev_cnt = 0;
	pdic_data->scr_sel = PLUG_CTRL_RP80;
	pdic_data->rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	pdic_data->is_cc_abnormal_state = false;
#if defined(CONFIG_SM5714_SUPPORT_SBU)
	pdic_data->is_sbu_abnormal_state = false;
#endif
	pdic_data->pd_support = 0;
	pdic_data->suspended = false;
	init_waitqueue_head(&pdic_data->suspend_wait);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	wakeup_source_init(pdic_data->irq_ws, "irq_wake");   // 4.19 R
	if (!(pdic_data->irq_ws)) {
		pdic_data->irq_ws = wakeup_source_create("irq_wake"); // 4.19 Q
		if (pdic_data->irq_ws)
			wakeup_source_add(pdic_data->irq_ws);
	}
#else
	pdic_data->irq_ws = wakeup_source_register(NULL, "irq_wake"); // 5.4 R
#endif
	ret = sm5714_usbpd_init(dev, pdic_data);
	if (ret < 0) {
		dev_err(dev, "failed on usbpd_init\n");
		goto err_return;
	}

	sm5714_usbpd_set_ops(dev, &sm5714_ops);

	mutex_init(&pdic_data->_mutex);
	mutex_init(&pdic_data->lpm_mutex);

	sm5714_usbpd_reg_init(pdic_data);
#if defined(CONFIG_SEC_FACTORY)
	INIT_DELAYED_WORK(&pdic_data->factory_state_work,
		sm5714_factory_check_abnormal_state);
	INIT_DELAYED_WORK(&pdic_data->factory_rid_work,
		sm5714_factory_check_normal_rid);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER_SM5714)
	INIT_DELAYED_WORK(&pdic_data->vbus_noti_work, sm5714_usbpd_handle_vbus);
#endif
	INIT_DELAYED_WORK(&pdic_data->vbus_dischg_work,
			sm5714_vbus_dischg_work);
	INIT_DELAYED_WORK(&pdic_data->debug_work, sm5714_usbpd_debug_reg_log);
	schedule_delayed_work(&pdic_data->debug_work, msecs_to_jiffies(10000));

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	pdic_register_switch_device(1);
	/* Create a work queue for the pdic irq thread */
	pdic_data->pdic_wq
		= create_singlethread_workqueue("pdic_irq_event");
	if (!pdic_data->pdic_wq) {
		pr_err("%s failed to create work queue for pdic notifier\n",
			__func__);
		goto err_return;
	}
	if (pdic_data->rid == REG_RID_UNDF)
		pdic_data->rid = REG_RID_MAX;
	ppdic_data = kzalloc(sizeof(pdic_data_t), GFP_KERNEL);
	ppdic_sysfs_prop = kzalloc(sizeof(pdic_sysfs_property_t), GFP_KERNEL);
	ppdic_sysfs_prop->get_property = sm5714_sysfs_get_prop;
	ppdic_sysfs_prop->set_property = sm5714_sysfs_set_prop;
	ppdic_sysfs_prop->property_is_writeable = sm5714_sysfs_is_writeable;
	ppdic_sysfs_prop->properties = sm5714_sysfs_properties;
	ppdic_sysfs_prop->num_properties = ARRAY_SIZE(sm5714_sysfs_properties);
	ppdic_data->pdic_sysfs_prop = ppdic_sysfs_prop;
	ppdic_data->drv_data = pdic_data;
	ppdic_data->name = "sm5714";
	pdic_core_register_chip(ppdic_data);
	ret = pdic_misc_init(ppdic_data);
	if (ret) {
		pr_err("pdic_misc_init is failed, error %d\n", ret);
	} else {
		ppdic_data->misc_dev->uvdm_read = sm5714_usbpd_uvdm_in_request_message;
		ppdic_data->misc_dev->uvdm_write = sm5714_usbpd_uvdm_out_request_message;
		ppdic_data->misc_dev->uvdm_ready = sm5714_usbpd_uvdm_ready;
		ppdic_data->misc_dev->uvdm_close = sm5714_usbpd_uvdm_close;
		pdic_data->ppdic_data = ppdic_data;
	}
#endif

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	usbpd_d = kzalloc(sizeof(struct usbpd_dev), GFP_KERNEL);
	if (!usbpd_d) {
		dev_err(dev, "%s: failed to allocate usbpd data\n", __func__);
		ret = -ENOMEM;
		goto err_kfree1;
	}

	usbpd_d->ops = &ops_usbpd;
	usbpd_d->data = (void *)pdic_data;
	pdic_data->man = register_usbpd(usbpd_d);
#endif

#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SM5714_WATER_DETECTION_ENABLE)
	sm5714_power_off_water_check(pdic_data);
#endif

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	pdic_data->data_role_dual = 0;
	pdic_data->power_role_dual = 0;
	desc =
		devm_kzalloc(&i2c->dev,
				sizeof(struct dual_role_phy_desc), GFP_KERNEL);
	if (!desc) {
		pr_err("unable to allocate dual role descriptor\n");
		goto fail_init_irq;
	}

	desc->name = "otg_default";
	desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
	desc->get_property = sm5714_dual_role_get_prop;
	desc->set_property = sm5714_dual_role_set_prop;
	desc->properties = sm5714_fusb_drp_properties;
	desc->num_properties = ARRAY_SIZE(sm5714_fusb_drp_properties);
	desc->property_is_writeable = sm5714_dual_role_is_writeable;
	dual_role =
		devm_dual_role_instance_register(&i2c->dev, desc);
	dual_role->drv_data = pdic_data;
	pdic_data->dual_role = dual_role;
	pdic_data->desc = desc;
	init_completion(&pdic_data->reverse_completion);
#elif defined(CONFIG_TYPEC)
	pdic_data->typec_cap.revision = USB_TYPEC_REV_1_2;
	pdic_data->typec_cap.pd_revision = 0x300;
	pdic_data->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
#if !defined(CONFIG_SUPPORT_USB_TYPEC_OPS)
	pdic_data->typec_cap.pr_set = sm5714_pr_set;
	pdic_data->typec_cap.dr_set = sm5714_dr_set;
	pdic_data->typec_cap.port_type_set = sm5714_port_type_set;
#else
	pdic_data->typec_cap.driver_data = pdic_data;
	pdic_data->typec_cap.ops = &sm5714_typec_ops;
#endif
	pdic_data->typec_cap.type = TYPEC_PORT_DRP;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	pdic_data->typec_cap.data = TYPEC_PORT_DRP;
#endif
	pdic_data->typec_power_role = TYPEC_SINK;
	pdic_data->typec_data_role = TYPEC_DEVICE;
	pdic_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
	pdic_data->port = typec_register_port(pdic_data->dev,
		&pdic_data->typec_cap);
	if (IS_ERR(pdic_data->port))
		pr_err("%s : unable to register typec_register_port\n", __func__);
	else
		pr_info("%s : success typec_register_port\n", __func__);
	init_completion(&pdic_data->typec_reverse_completion);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
	INIT_DELAYED_WORK(&pdic_data->role_swap_work, sm5714_role_swap_check);
	init_waitqueue_head(&pdic_data->host_turn_on_wait_q);
	pdic_data->host_turn_on_wait_time = 20;
	ret = sm5714_usbpd_irq_init(pdic_data);
	if (ret) {
		dev_err(dev, "%s: failed to init irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	device_init_wakeup(dev, 1);
	/* initial cable detection */
	sm5714_pdic_irq_thread(-1, pdic_data);
	INIT_DELAYED_WORK(&pdic_data->usb_external_notifier_register_work,
				  sm5714_delayed_external_notifier_init);
	/* Register pdic handler to pdic notifier block list */
	ret = usb_external_notify_register(&pdic_data->usb_external_notifier_nb,
			sm5714_handle_usb_external_notifier_notification,
			EXTERNAL_NOTIFY_DEV_PDIC);
	if (ret < 0)
		schedule_delayed_work(&pdic_data->usb_external_notifier_register_work,
			msecs_to_jiffies(2000));
	else
		pr_info("%s : external notifier register done!\n", __func__);
	init_completion(&pdic_data->exit_mpsm_completion);
	pr_info("%s : sm5714 usbpd driver uploaded!\n", __func__);
	return 0;
fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, pdic_data);
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	kfree(usbpd_d);
err_kfree1:
#endif
	kfree(pdic_data);
err_return:
	return ret;
}

#if defined CONFIG_PM
static int sm5714_usbpd_suspend(struct device *dev)
{
	struct sm5714_usbpd_data *_data = dev_get_drvdata(dev);
	struct sm5714_phydrv_data *pdic_data = _data->phy_driver_data;

	pdic_data->suspended = true;

	if (device_may_wakeup(dev))
		enable_irq_wake(pdic_data->i2c->irq);

#if !defined(CONFIG_ARCH_QCOM_NO) && !defined(CONFIG_ARCH_MEDIATEK)
	disable_irq(pdic_data->i2c->irq);
#endif
	if (pdic_data->is_timer_expired) {
		pdic_data->abnormal_dev_cnt = 0;
		sm5714_usbpd_write_reg(pdic_data->i2c, SM5714_REG_GEN_TMR_L, 0x00);
		sm5714_usbpd_write_reg(pdic_data->i2c, SM5714_REG_GEN_TMR_U, 0x00);
	}

	cancel_delayed_work_sync(&pdic_data->debug_work);

	return 0;
}

static int sm5714_usbpd_resume(struct device *dev)
{
	struct sm5714_usbpd_data *_data = dev_get_drvdata(dev);
	struct sm5714_phydrv_data *pdic_data = _data->phy_driver_data;

	pdic_data->suspended = false;
#if defined(CONFIG_ARCH_QCOM_NO) || defined(CONFIG_ARCH_MEDIATEK)
	wake_up(&pdic_data->suspend_wait);
#endif

	if (device_may_wakeup(dev))
		disable_irq_wake(pdic_data->i2c->irq);

#if !defined(CONFIG_ARCH_QCOM_NO) && !defined(CONFIG_ARCH_MEDIATEK)
	enable_irq(pdic_data->i2c->irq);
#endif
	if (pdic_data->is_timer_expired) {
		sm5714_abnormal_dev_int_on_off(pdic_data, 1);
		pdic_data->is_timer_expired = false;
	}
	schedule_delayed_work(&pdic_data->debug_work, msecs_to_jiffies(1500));

	return 0;
}
#endif

static int sm5714_usbpd_remove(struct i2c_client *i2c)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(&i2c->dev);
	struct sm5714_phydrv_data *_data = pd_data->phy_driver_data;

	if (_data) {
		cancel_delayed_work_sync(&_data->debug_work);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		devm_dual_role_instance_unregister(_data->dev,
		_data->dual_role);
		devm_kfree(_data->dev, _data->desc);
#elif defined(CONFIG_TYPEC)
		typec_unregister_port(_data->port);
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		pdic_register_switch_device(0);
		if (_data->ppdic_data && _data->ppdic_data->misc_dev)
			pdic_misc_exit();
#endif
		disable_irq_wake(_data->i2c->irq);
		free_irq(_data->i2c->irq, _data);
		gpio_free(_data->irq_gpio);
		mutex_destroy(&_data->_mutex);
		sm5714_usbpd_set_vbus_dischg_gpio(_data, 0);
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
		kfree(_data->man->usbpd_d);
#endif
		kfree(_data);
	}
	return 0;
}

static const struct i2c_device_id sm5714_usbpd_i2c_id[] = {
	{ USBPD_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, sm5714_usbpd_i2c_id);

static const struct of_device_id sec_usbpd_i2c_dt_ids[] = {
	{ .compatible = "sm5714-usbpd" },
	{ }
};

static void sm5714_usbpd_shutdown(struct i2c_client *i2c)
{
	struct sm5714_usbpd_data *pd_data = dev_get_drvdata(&i2c->dev);
	struct sm5714_phydrv_data *_data = pd_data->phy_driver_data;
	bool is_rid_attached = true;
	u8 data;

	if (_data->rid == REG_RID_OPEN || _data->rid == REG_RID_MAX)
		is_rid_attached = false;

	if (!_data->i2c)
		return;

	cancel_delayed_work_sync(&_data->debug_work);
	sm5714_usbpd_set_vbus_dischg_gpio(_data, 0);

	if (_data->pd_support && !is_rid_attached) {
		sm5714_usbpd_read_reg(i2c, SM5714_REG_CC_CNTL3, &data);
		data |= 0x04; /* go to ErrorRecovery State */
		sm5714_usbpd_write_reg(i2c, SM5714_REG_CC_CNTL3, data);
	}

	if (!is_rid_attached)
		sm5714_usbpd_write_reg(i2c, SM5714_REG_SYS_CNTL, 0x80);
}

static usbpd_phy_ops_type sm5714_ops = {
	.tx_msg			= sm5714_tx_msg,
	.rx_msg			= sm5714_rx_msg,
	.hard_reset		= sm5714_hard_reset,
	.set_power_role		= sm5714_set_power_role,
	.get_power_role		= sm5714_get_power_role,
	.set_data_role		= sm5714_set_data_role,
	.get_data_role		= sm5714_get_data_role,
	.set_vconn_source	= sm5714_set_vconn_source,
	.get_vconn_source	= sm5714_get_vconn_source,
	.set_check_msg_pass	= sm5714_set_check_msg_pass,
	.get_status		= sm5714_get_status,
	.poll_status		= sm5714_poll_status,
	.driver_reset		= sm5714_driver_reset,
	.get_short_state	= sm5714_get_short_state,
};

#if defined CONFIG_PM
const struct dev_pm_ops sm5714_usbpd_pm = {
	.suspend = sm5714_usbpd_suspend,
	.resume = sm5714_usbpd_resume,
};
#endif

static struct i2c_driver sm5714_usbpd_driver = {
	.driver		= {
		.name	= USBPD_DEV_NAME,
		.of_match_table	= sec_usbpd_i2c_dt_ids,
#if defined CONFIG_PM
		.pm	= &sm5714_usbpd_pm,
#endif /* CONFIG_PM */
	},
	.probe		= sm5714_usbpd_probe,
	.remove		= sm5714_usbpd_remove,
	.shutdown	= sm5714_usbpd_shutdown,
	.id_table	= sm5714_usbpd_i2c_id,
};

static int __init sm5714_usbpd_typec_init(void)
{
	return i2c_add_driver(&sm5714_usbpd_driver);
}
late_initcall(sm5714_usbpd_typec_init);

static void __exit sm5714_usbpd_typec_exit(void)
{
	i2c_del_driver(&sm5714_usbpd_driver);
}
module_exit(sm5714_usbpd_typec_exit);

MODULE_DESCRIPTION("SM5714 USB TYPE-C driver");
MODULE_LICENSE("GPL");
