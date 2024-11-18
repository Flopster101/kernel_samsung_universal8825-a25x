/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#if defined(CONFIG_SENSORS_CORE_AP)
#include <linux/sensor/sensors_core.h>
#endif
#include <linux/vmalloc.h>
#if IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#define HALL_NAME		"hall"
#define HALL_CERT_NAME		"certify_hall"
#define HALL_FLIP_NAME		"flip"
#define HALL_ATTACH		1
#define HALL_DETACH		0
#endif

#include "isg5320a_reg.h"

#define CHIP_ID                 0x32
#define VENDOR_NAME             "IMAGIS"
#define ISG5320A_MODE_SLEEP      0
#define ISG5320A_MODE_NORMAL     1
#define ISG5320A_DIFF_AVR_CNT    10
#define ISG5320A_DISPLAY_TIME    30

#define ISG5320A_INIT_DELAYEDWORK
#define GRIP_LOG_TIME            40 /* 20 sec */

#define TYPE_USB   1
#define TYPE_HALL  2
#define TYPE_BOOT  3
#define TYPE_FORCE 4
#define TYPE_COVER 5

#pragma pack(1)
typedef struct {
	char cmd;
	u8 addr;
	u8 val;
} direct_info;
#pragma pack()

struct isg5320a_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *noti_input_dev;
	struct device *dev;
	struct delayed_work debug_work;
	struct delayed_work cal_work;
	struct work_struct cfcal_work;
	struct work_struct bfcal_work;
#ifdef ISG5320A_INIT_DELAYEDWORK
	struct delayed_work init_work;
#endif
	struct wakeup_source *grip_ws;
	struct mutex lock;
#if IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct notifier_block pdic_nb;
	int pdic_status;
	int pdic_pre_attach;
	int pre_otg_attach;
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	struct notifier_block hall_nb;
#endif
	direct_info direct;

	int gpio_int;
	int enable;
	int noti_enable;
	int state;

	int diff_cnt;
	int diff_sum;
	int diff_avg;
	int cdc_sum;
	int cdc_avg;
	int initialized;

	int reg_size;

	int pre_attach;

	int intr_debug_size;
	int debug_cnt;
	int irq_count;
	int abnormal_mode;

	u32 cdc;
	u32 base;
	s32 diff;
	s32 max_diff;
	s32 max_normal_diff;

	u32 debug_cdc[3];
	u32 debug_base[2];
	s32 debug_diff[3];

	u32 cfcal_th;
	u32 bfcal_chk_count;
	u32 bfcal_chk_cdc;
	s32 bfcal_chk_diff;

	u16 normal_th;
	u16 fine_coarse;

	u8 setup_reg[320];
	u8 intr_debug_addr;
	u8 debug_val[3];
	u8 ic_num;

	bool skip_data;
	bool bfcal_chk_ready;
	bool bfcal_chk_start;
	bool setup_reg_exist;
	bool cal_done_flag;
	bool cdc_ret_flag;

	bool i2c_fail_err;
	bool reg_err;

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	u8 freq_step;
	u8 freq_value;
#endif
	int is_unknown_mode;
	int motion;
	bool first_working;
};

static int isg5320a_i2c_write(struct isg5320a_data *data, u8 cmd, u8 *val)
{
	int ret;
	u8 buf[3];
	struct i2c_msg msg;
	int retry = 0;
	buf[0] = cmd;
	memcpy(buf + sizeof(cmd), val, 2);

	msg.addr = data->client->addr;
	msg.flags = 0; /*I2C_M_WR*/
	msg.len = 3;
	msg.buf = buf;

	while (retry < 3) {
		ret = i2c_transfer(data->client->adapter, &msg, 1);
		if (ret == 1)
			break;
		GRIP_ERR("i2c_transfer failed(%d) retry : %d\n", ret, retry);
		usleep_range(5000, 5100);
		retry++;
	}
	return ret;
}

static int isg5320a_i2c_write_one(struct isg5320a_data *data, u8 cmd, u8 val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg;
	int retry = 0;
	buf[0] = cmd;
	buf[1] = val;

	msg.addr = data->client->addr;
	msg.flags = 0; /*I2C_M_WR*/
	msg.len = 2;
	msg.buf = buf;

	while (retry < 3) {
		ret = i2c_transfer(data->client->adapter, &msg, 1);
		if (ret == 1)
			break;
		GRIP_ERR("i2c_transfer failed(%d) retry : %d\n", ret, retry);
		data->i2c_fail_err = true;
		usleep_range(5000, 5100);
		retry++;
	}
	return ret;
}

static int isg5320a_i2c_read(struct isg5320a_data *data, u8 cmd, u8 *val,
			     int len)
{
	int ret;
	int retry = 0;
	struct i2c_msg msgs[2] = {
		{
			.addr = data->client->addr,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = &cmd,
		},
		{
			.addr = data->client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		},
	};

	while (retry < 3) {
		ret = i2c_transfer(data->client->adapter, msgs, 2);
		if (ret == 2)
			break;
		GRIP_ERR("i2c_transfer failed(%d) retry : %d\n", ret, retry);
		data->i2c_fail_err = true;
		usleep_range(5000, 5100);
		retry++;
	}

	return ret;
}

static int isg5320a_reset(struct isg5320a_data *data)
{
	int ret = 0;
	int cnt = 0;
	u8 val;

	GRIP_INFO("\n");

	if (data->initialized == OFF)
		usleep_range(5000, 5100);

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &val, 1);
	if (ret < 0) {
		GRIP_ERR("irq to high failed(%d)\n", ret);
		return ret;
	}

	while (gpio_get_value_cansleep(data->gpio_int) == 0 && cnt++ < 10)
		usleep_range(5000, 5100);

	if (cnt >= 10)
		GRIP_ERR("wait irq to high failed\n");

	ret = isg5320a_i2c_write_one(data, ISG5320A_PROTECT_REG,
				     ISG5320A_PRT_VALUE);
	if (ret < 0) {
		GRIP_ERR("unlock protect failed(%d)\n", ret);
		return ret;
	}

	ret = isg5320a_i2c_write_one(data, ISG5320A_SOFTRESET_REG,
				     ISG5320A_RST_VALUE);
	if (ret < 0) {
		GRIP_ERR("soft reset failed(%d)\n", ret);
		return ret;
	}

	usleep_range(1000, 1100);

	cnt = 0;
	while (gpio_get_value_cansleep(data->gpio_int) != 0 && cnt++ < 10)
		usleep_range(5000, 5100);

	if (cnt >= 10) {
		GRIP_ERR("wait soft reset failed\n");
		return -EIO;
	} else if (cnt != 1) {
		GRIP_INFO("wait cnt:%d\n", cnt);
	}

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &val, 1);
	if (ret < 0) {
		GRIP_ERR("irq to high failed(%d)\n", ret);
		return ret;
	}

	isg5320a_i2c_write_one(data, 0x13, 0xBA);
	return ret;
}

static void isg5320a_force_calibration(struct isg5320a_data *data,
				       bool only_bfcal)

{
	int retry = 3;
	int ret;
	u8 calbuf = 0;

	mutex_lock(&data->lock);
	GRIP_INFO("(%d)\n", only_bfcal ? 1 : 0);
	while (retry--) {
		if (!only_bfcal) {
			isg5320a_i2c_write_one(data, 0x38, 0x80);
			usleep_range(10000, 10100);
			isg5320a_i2c_write_one(data, 0x38, 0x00);
			isg5320a_i2c_write_one(data, 0x19, 0x0A);
			usleep_range(10000, 10100);
			isg5320a_i2c_write_one(data, 0xCD, 0xDE);
			isg5320a_i2c_write_one(data, 0xC9, 0x10);
			usleep_range(10000, 10100);
			isg5320a_i2c_write_one(data, 0xCD, 0xDE);
			isg5320a_i2c_write_one(data, 0xC9, 0x00);
			usleep_range(10000, 10100);
			isg5320a_i2c_write_one(data, 0x19, 0x8A);
			isg5320a_i2c_write_one(data, 0x38, 0xDD);
			msleep(400);
		}

		isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL2_REG, ISG5320A_BFCAL_START);
		msleep(100);
		ret = isg5320a_i2c_read(data, ISG5320A_CFCAL_RTN_REG, &calbuf, 1);
		if (ret < 0)
			GRIP_INFO("fail to get cal done flag\n");

		data->cal_done_flag = calbuf & 1;
		if (data->cal_done_flag)
			break;
		GRIP_INFO("cal_retry %d\n", retry);
	}
	mutex_unlock(&data->lock);
}

static inline unsigned char str2int(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;

	return 0;
}

static int isg5320a_get_raw_data(struct isg5320a_data *data, bool log_print)
{
	int ret = 0;
	u8 buf[4];
	u8 calbuf;
	u16 cpbuf;
	u32 temp;

	ret = isg5320a_i2c_read(data, ISG5320A_CFCAL_RTN_REG, &calbuf, 1);
	if (ret < 0)
		GRIP_INFO("fail to get cal done flag\n");

	data->cal_done_flag = calbuf & 1;
	if (!data->cal_done_flag) {
		GRIP_INFO("under calibration\n");
		data->diff = 0;
		data->cdc = 0;
		data->base = 0;
		data->fine_coarse = 0;
		return ret;
	}

	mutex_lock(&data->lock);
	data->cdc_ret_flag = false;
	ret = isg5320a_i2c_read(data, ISG5320A_CDC16_T_H_REG, buf, sizeof(buf));

	if (ret < 0) {
		GRIP_INFO("fail to get data\n");
	} else {
		temp = ((u32)buf[2] << 8) | (u32)buf[3];
		if ((temp != 0) && (temp != 0xFFFF) && (temp > 500)) {
			data->base = temp;
			temp = ((u32)buf[0] << 8) | (u32)buf[1];
			if ((temp != 0) && (temp != 0xFFFF))
				data->cdc = temp;
			else if (temp == 0)
				data->cdc_ret_flag = true;
			data->diff = (s32)data->cdc - (s32)data->base;
		} else if (temp == 0) {
			data->cdc_ret_flag = true;
		}
		ret = isg5320a_i2c_read(data, ISG5320A_COARSE_OUT_B_REG, (u8 *)&cpbuf, 2);
		if (ret < 0)
			GRIP_INFO("fail to get capMain\n");
		else
			data->fine_coarse = cpbuf;
	}

	mutex_unlock(&data->lock);

	if (log_print) {
		GRIP_INFO("capMain: %d%02d, cdc: %d, baseline:%d, diff:%d, skip_data:%d\n",
			(data->fine_coarse & 0xFF),
			((data->fine_coarse >> 8) & 0x3F), data->cdc, data->base,
			data->diff, data->skip_data);
	} else {
		if (data->debug_cnt >= GRIP_LOG_TIME) {
			GRIP_INFO("capMain: %d%02d, cdc: %d, baseline:%d, diff:%d, skip_data:%d\n",
				(data->fine_coarse & 0xFF),
				((data->fine_coarse >> 8) & 0x3F), data->cdc, data->base,
				data->diff, data->skip_data);
			data->debug_cnt = 0;
		} else {
			data->debug_cnt++;
		}
	}
	return ret;
}

static void force_far_grip(struct isg5320a_data *data)
{
	if (data->state == CLOSE) {
		GRIP_INFO("\n");

		if (data->skip_data == true)
			return;

		input_report_rel(data->input_dev, REL_MISC, 2);
		input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);
		input_sync(data->input_dev);
		data->state = FAR;
	}
}

static void report_event_data(struct isg5320a_data *data, u8 intr_msg)
{
	int state;

	if (data->skip_data == true) {
		GRIP_INFO("skip grip event\n");
		return;
	}

	state = (intr_msg & (1 << ISG5320A_PROX_STATE)) ? CLOSE : FAR;

	if (data->abnormal_mode) {
		if (state == CLOSE) {
			if (data->max_diff < data->diff)
				data->max_diff = data->diff;
			data->irq_count++;
		}
	}

	if (state == CLOSE) {
		if (data->state == FAR) {
			GRIP_INFO("CLOSE\n");
			data->state = CLOSE;
			data->bfcal_chk_start = true;
			data->bfcal_chk_ready = false;
			data->bfcal_chk_count = 0;
		} else {
			GRIP_INFO("still CLOSE\n");
		}
	} else {
		if (data->state == CLOSE) {
			GRIP_INFO("FAR\n");
			data->state = FAR;
			data->bfcal_chk_start = false;
			data->bfcal_chk_ready = false;
			data->bfcal_chk_count = 0;
		} else {
			GRIP_INFO("already FAR\n");
		}
	}

	if (data->state == CLOSE) {
		input_report_rel(data->input_dev, REL_MISC, 1);
		if (data->is_unknown_mode == UNKNOWN_ON && data->motion)
			data->first_working = true;
	} else {
		input_report_rel(data->input_dev, REL_MISC, 2);
		if (data->is_unknown_mode == UNKNOWN_ON && data->motion) {
			if (data->first_working) {
				GRIP_INFO("unknown mode off\n");
				data->is_unknown_mode = UNKNOWN_OFF;
				data->first_working = false;
			}
		}
	}

	input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);
	input_sync(data->input_dev);
}

static irqreturn_t isg5320a_irq_thread(int irq, void *ptr)
{
	int ret;
	int i;
	u8 intr_msg = 0;
	u8 *buf8;
	struct isg5320a_data *data = (struct isg5320a_data *)ptr;

	if (data->initialized == OFF)
		return IRQ_HANDLED;

	__pm_stay_awake(data->grip_ws);

	ret = isg5320a_get_raw_data(data, true);
	if (ret < 0) {
		GRIP_ERR("fail to read state(%d)\n", ret);
		goto irq_end;
	}

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &intr_msg, 1);
	if (ret < 0) {
		GRIP_ERR("fail to read state(%d)\n", ret);
		goto irq_end;
	}

	if (data->intr_debug_size > 0) {
		buf8 = kzalloc(data->intr_debug_size, GFP_KERNEL);
		if (buf8) {
			GRIP_INFO("Intr_debug1 (0x%02X)\n",
				data->intr_debug_addr);
			ret = isg5320a_i2c_read(data, data->intr_debug_addr, buf8,
					  data->intr_debug_size);
			if (ret < 0) {
				GRIP_ERR("fail to read state(%d)\n", ret);
				kfree(buf8);
				goto irq_end;
			}

			for (i = 0; i < data->intr_debug_size; i++)
				GRIP_INFO("\t%02X\n", buf8[i]);
			kfree(buf8);
		}
	}

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSTS_REG, &intr_msg, 1);
	if (ret < 0) {
		GRIP_ERR("fail to read state(%d)\n", ret);
		goto irq_end;
	}

	GRIP_INFO("intr msg: 0x%02X\n", intr_msg);

	report_event_data(data, intr_msg);

irq_end:
	__pm_relax(data->grip_ws);

	return IRQ_HANDLED;
}

static void isg5320a_enter_unknown_mode(struct isg5320a_data *data, int type)
{
	if (data->noti_enable && !data->skip_data) {
		data->motion = 0;
		data->first_working = false;
		if (data->is_unknown_mode == UNKNOWN_OFF) {
			data->is_unknown_mode = UNKNOWN_ON;
			input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);
			input_sync(data->input_dev);
			GRIP_INFO("UNKNOWN Re-enter\n");
		} else {
			GRIP_INFO("already UNKNOWN\n");
		}
		input_report_rel(data->noti_input_dev, REL_X, type);
		input_sync(data->noti_input_dev);
	}
}

static void isg5320a_initialize(struct isg5320a_data *data)
{
	int ret;
	int i;
	u8 val;
	u8 buf[2];
	u8 buf8[2];

	GRIP_INFO("\n");
	mutex_lock(&data->lock);
	force_far_grip(data);

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &val, 1);
	if (ret < 0) {
		GRIP_ERR("i2c read fail(%d)\n", ret);
		mutex_unlock(&data->lock);
		return;
	}
	ret = isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL1_REG, ISG5320A_SCAN_STOP);
	if (ret < 0) {
		GRIP_ERR("i2c write fail(%d)\n", ret);
		mutex_unlock(&data->lock);
		return;
	}
	msleep(30);

	if (data->setup_reg_exist) {
		for (i = 0; i < data->reg_size ; i++) {
			int index = i * 2;

			if (data->setup_reg[index] == ISG5320A_IBAS_REG)
				data->debug_val[0] = data->setup_reg[index + 1];
			else if (data->setup_reg[index] == ISG5320A_THD_REG)
				data->debug_val[1] = data->setup_reg[index + 1];
			else if (data->setup_reg[index] == ISG5320A_TARGET_CDC_REG)
				data->debug_val[2] = data->setup_reg[index + 1];

			isg5320a_i2c_write_one(data, data->setup_reg[index], data->setup_reg[index + 1]);
#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_TEST_FOR_GRIP)  
			if ((data->setup_reg[index] != setup_reg[i].addr)
				|| (data->setup_reg[index + 1] != setup_reg[i].val))
			{
				GRIP_INFO("%02X %02X\n", data->setup_reg[index],
					data->setup_reg[index + 1]);				
				GRIP_INFO("%02X %02X\n", setup_reg[i].addr,
					setup_reg[i].val);
			}
/*
			isg5320a_i2c_read(data, data->setup_reg[index], &val, 1);
			GRIP_INFO("%02X %02X\n", data->setup_reg[index], val);
*/
#endif
		}
	}

	if (data->normal_th > 0) {
		buf[0] = (data->normal_th >> 8) & 0xFF;
		buf[1] = data->normal_th & 0xFF;

		isg5320a_i2c_write(data, ISG5320A_B_PROXCTL3_REG, buf);
	}

	ret = isg5320a_i2c_read(data, ISG5320A_DIGITAL_ACC_REG, &val, 1);
	if (ret < 0)
		GRIP_ERR("fail to read DIGITAL ACC(%d)\n", ret);
	else
		data->cfcal_th = ISG5320A_RESET_CONDITION * val / 8;
	mutex_unlock(&data->lock);
	data->initialized = ON;

	isg5320a_i2c_read(data, ISG5320A_B_PROXCTL3_REG, buf8, sizeof(buf8));
	data->normal_th = ((u32)buf8[0] << 8) | (u32)buf8[1];
}

static void isg5320a_set_debug_work(struct isg5320a_data *data, bool enable,
				    unsigned int delay_ms)
{
	if (enable == ON) {
		data->debug_cnt = 0;
		schedule_delayed_work(&data->debug_work, msecs_to_jiffies(delay_ms));
		schedule_delayed_work(&data->cal_work, msecs_to_jiffies(delay_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
		cancel_delayed_work_sync(&data->cal_work);
	}
}

static void isg5320a_set_enable(struct isg5320a_data *data, int enable)
{
	u8 state;
	int ret = 0;

	GRIP_INFO("\n");

	if (data->enable == enable) {
		GRIP_INFO("already enabled\n");
		return;
	}

	if (enable == ON) {
		GRIP_INFO("enable\n");

		data->diff_avg = 0;
		data->diff_cnt = 0;
		data->cdc_avg = 0;

		ret = isg5320a_i2c_read(data, ISG5320A_IRQSTS_REG, &state, 1);
		if (ret < 0) {
			GRIP_ERR("i2c read fail(%d)\n", ret);
			return;
		}

		isg5320a_get_raw_data(data, true);

		if (data->skip_data == true) {
			input_report_rel(data->input_dev, REL_MISC, 2);
			input_report_rel(data->input_dev, REL_X, UNKNOWN_OFF);
		} else if (state & (1 << ISG5320A_PROX_STATE)) {
			data->state = CLOSE;
			input_report_rel(data->input_dev, REL_MISC, 1);
			input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);
		} else {
			data->state = FAR;
			input_report_rel(data->input_dev, REL_MISC, 2);
			input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);
		}
		input_sync(data->input_dev);

		isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &state, 1);
		isg5320a_i2c_write_one(data, ISG5320A_IRQFUNC_REG, ISG5320A_IRQ_ENABLE);

		enable_irq(data->client->irq);
		enable_irq_wake(data->client->irq);
	} else {
		GRIP_INFO("disable\n");

		ret = isg5320a_i2c_write_one(data, ISG5320A_IRQFUNC_REG, ISG5320A_IRQ_DISABLE);
		if (ret < 0) {
			GRIP_ERR("i2c write fail(%d)\n", ret);
			return;
		}

		disable_irq(data->client->irq);
		disable_irq_wake(data->client->irq);
	}

	data->enable = enable;
}

static int isg5320a_set_mode(struct isg5320a_data *data, unsigned char mode)
{
	int ret = -EINVAL;
	u8 state;

	ret = isg5320a_i2c_read(data, ISG5320A_IRQSRC_REG, &state, 1);
	if (ret < 0) {
		GRIP_ERR("i2c read fail(%d)\n", ret);
		return ret;
	}

	if (mode == ISG5320A_MODE_SLEEP) {
		isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL1_REG,
				       ISG5320A_SCAN_STOP);
		isg5320a_i2c_write_one(data, ISG5320A_OSCCON_REG, ISG5320A_OSC_SLEEP);
		isg5320a_i2c_write_one(data, ISG5320A_BS_ON_WD_REG, ISG5320A_BS_WD_OFF);
	} else if (mode == ISG5320A_MODE_NORMAL) {
		isg5320a_i2c_write_one(data, ISG5320A_BS_ON_WD_REG, ISG5320A_BS_WD_ON);
		isg5320a_force_calibration(data, false);
	}

	GRIP_INFO("change the mode : %u\n", mode);

	return ret;
}

static ssize_t isg5320a_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	GRIP_INFO("%s\n", isg5320a_device_name[data->ic_num]);

	return sprintf(buf, "%s\n", isg5320a_device_name[data->ic_num]);
}

static ssize_t isg5320a_vendor_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	GRIP_INFO("%s\n", VENDOR_NAME);

	return sprintf(buf, "%s\n", VENDOR_NAME);
}

static ssize_t isg5320a_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

static ssize_t isg5320a_acal_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "2,0,0\n");
}

static ssize_t isg5320a_manual_acal_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "OK\n");
}

static ssize_t isg5320a_onoff_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("invalid argument\n");
		return ret;
	}

	if (val == 0) {
		data->skip_data = true;
		if (data->enable == ON) {
			data->state = FAR;
			input_report_rel(data->input_dev, REL_MISC, 2);
			input_report_rel(data->input_dev, REL_X, UNKNOWN_OFF);
			input_sync(data->input_dev);
		}
		data->motion = 1;
		data->is_unknown_mode = UNKNOWN_OFF;
		data->first_working = false;
	} else {
		data->skip_data = false;
	}

	GRIP_INFO("%d\n", (int)val);

	return count;
}

static ssize_t isg5320a_onoff_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", !data->skip_data);
}

static ssize_t isg5320a_sw_reset_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 debug_buf[3];

	GRIP_INFO("\n");
	cancel_delayed_work_sync(&data->cal_work);

	//for check if registers need recovery
	isg5320a_i2c_read(data, ISG5320A_IBAS_REG, &debug_buf[0], sizeof(debug_buf[0]));
	isg5320a_i2c_read(data, ISG5320A_THD_REG, &debug_buf[1], sizeof(debug_buf[1]));
	isg5320a_i2c_read(data, ISG5320A_TARGET_CDC_REG, &debug_buf[2], sizeof(debug_buf[2]));

	if (debug_buf[0] != data->debug_val[0] || debug_buf[1] != data->debug_val[1] || debug_buf[2] != data->debug_val[2]) {
		GRIP_ERR("sw_reset initial");
		isg5320a_reset(data);
		isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);
		isg5320a_initialize(data);
		isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);
		data->reg_err = true;
	} else {
#if defined(CONFIG_SENSORS_ISG5320A_USE_BFCAL)
		isg5320a_force_calibration(data, true);
#else
		isg5320a_force_calibration(data, false);
#endif
	}
	isg5320a_get_raw_data(data, true);

	schedule_delayed_work(&data->cal_work, msecs_to_jiffies(1000));

	if (!data->cal_done_flag)
		return sprintf(buf, "-1\n");

	return sprintf(buf, "%d\n", 0);
}

static ssize_t isg5320a_normal_threshold_store(struct device *dev,
					       struct device_attribute *attr, const char *buf, size_t size)
{
	int val = 0;
	u8 buf8[2];
	struct isg5320a_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d", &val);

	if (val < 0) {
		GRIP_ERR("invalid argument\n");
		return size;
	}

	GRIP_INFO("change threshold(%d->%d)\n", data->normal_th, val);

	data->normal_th = val;

	buf8[0] = (data->normal_th >> 8) & 0xFF;
	buf8[1] = data->normal_th & 0xFF;

	isg5320a_i2c_write(data, ISG5320A_B_PROXCTL3_REG, buf8);

	return size;
}

static ssize_t isg5320a_normal_threshold_show(struct device *dev,
					      struct device_attribute *attr, char *buf)
{
	u32 threshold = 0;
	u32 close_hyst = 0;
	u32 far_hyst = 0;
	u8 buf8[6];
	struct isg5320a_data *data = dev_get_drvdata(dev);

	isg5320a_i2c_read(data, ISG5320A_B_PROXCTL3_REG, buf8, sizeof(buf8));

	threshold = ((u32)buf8[0] << 8) | (u32)buf8[1];
	close_hyst = ((u32)buf8[2] << 8) | (u32)buf8[3];
	far_hyst = ((u32)buf8[4] << 8) | (u32)buf8[5];

	return sprintf(buf, "%d,%d\n", threshold + close_hyst,
		       threshold - far_hyst);
}

static ssize_t isg5320a_raw_data_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	isg5320a_get_raw_data(data, true);
	if (!data->cal_done_flag) {
		data->diff_cnt = 0;
		data->diff_avg = 0;
		data->cdc_avg = 0;

		return sprintf(buf, "000,0,0,0,0\n");
	}
	if (data->diff_cnt == 0) {
		data->diff_sum = data->diff;
		data->cdc_sum = data->cdc;
	} else {
		data->diff_sum += data->diff;
		data->cdc_sum += data->cdc;
	}

	if (++data->diff_cnt >= ISG5320A_DIFF_AVR_CNT) {
		data->diff_avg = data->diff_sum / ISG5320A_DIFF_AVR_CNT;
		data->cdc_avg = data->cdc_sum / ISG5320A_DIFF_AVR_CNT;
		data->diff_cnt = 0;
	}

	return sprintf(buf, "%d%02d,%d,%d,%d,%d\n", (data->fine_coarse & 0xFF),
		       ((data->fine_coarse >> 8) & 0x3F), data->cdc,
		       data->fine_coarse, data->diff, data->base);
}

static ssize_t isg5320a_diff_avg_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->diff_avg);
}

static ssize_t isg5320a_cdc_avg_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->cdc_avg);
}

static ssize_t isg5320a_ch_state_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int count;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	if (data->skip_data == true)
		count = snprintf(buf, PAGE_SIZE, "%d,%d\n", NONE_ENABLE, NONE_ENABLE);
	else if (data->enable == ON)
		count = snprintf(buf, PAGE_SIZE, "%d,%d\n", data->state, NONE_ENABLE);
	else
		count = snprintf(buf, PAGE_SIZE, "%d,%d\n", NONE_ENABLE, NONE_ENABLE);

	return count;
}

static ssize_t isg5320a_hysteresis_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	u32 far_hyst = 0;
	u8 buf8[6];
	struct isg5320a_data *data = dev_get_drvdata(dev);

	isg5320a_i2c_read(data, ISG5320A_B_PROXCTL3_REG, buf8, sizeof(buf8));

	far_hyst = ((u32)buf8[4] << 8) | (u32)buf8[5];

	return sprintf(buf, "%d\n", far_hyst);
}

static ssize_t isg5320a_enable_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	u8 enable;
	struct isg5320a_data *data = dev_get_drvdata(dev);
	int pre_enable = data->enable;

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		GRIP_ERR("invalid argument\n");
		return size;
	}

	GRIP_INFO("new_value=%d old_value=%d\n", (int)enable, pre_enable);

	if (pre_enable == enable)
		return size;

	isg5320a_set_enable(data, (int)enable);

	return size;
}

static ssize_t isg5320a_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enable);
}

static ssize_t isg5320a_sampling_freq_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	int sampling_freq;

	isg5320a_i2c_read(data, ISG5320A_NUM_OF_CLK, &buff, 1);
	sampling_freq = (int)(4000 / ((int)buff + 1));

	return snprintf(buf, PAGE_SIZE, "%dkHz\n", sampling_freq);
}

static ssize_t isg5320a_isum_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	const char *table[16] = {
		"0", "0", "0", "0", "0", "0", "0", "0", "0",
		"20", "24", "28", "32", "40", "48", "64"
	};
	u8 buff = 0;

	isg5320a_i2c_read(data, ISG5320A_CHB_LSUM_TYPE_REG, &buff, 1);
	buff = (buff & 0xf0) >> 4;

	return snprintf(buf, PAGE_SIZE, "%s\n", table[buff]);
}

static ssize_t isg5320a_scan_period_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff[2];
	int scan_period;

	isg5320a_i2c_read(data, ISG5320A_WUTDATA_REG, (u8 *)&buff, sizeof(buff));

	scan_period = (int)(((u16)buff[1] & 0xff) | (((u16)buff[0] & 0x3f) << 8));
	if (!scan_period)
		return snprintf(buf, PAGE_SIZE, "%d\n", scan_period);

	scan_period = (int)(4000 / scan_period);

	return snprintf(buf, PAGE_SIZE, "%d\n", scan_period);
}

static ssize_t isg5320a_again_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	u8 temp1, temp2;

	isg5320a_i2c_read(data, ISG5320A_ANALOG_GAIN, &buff, 1);
	temp1 = (buff & 0x38) >> 3;
	temp2 = (buff & 0x07);

	return snprintf(buf, PAGE_SIZE, "%d/%d\n", (int)temp2 + 1, (int)temp1 + 1);
}

static ssize_t isg5320a_cdc_up_coef_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	int coef;

	isg5320a_i2c_read(data, ISG5320A_CHB_CDC_UP_COEF_REG, &buff, 1);
	coef = (int)buff;

	return snprintf(buf, PAGE_SIZE, "%x, %d\n", buff, coef);
}

static ssize_t isg5320a_cdc_down_coef_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;
	int coef;

	isg5320a_i2c_read(data, ISG5320A_CHB_CDC_DN_COEF_REG, &buff, 1);
	coef = (int)buff;

	return snprintf(buf, PAGE_SIZE, "%x, %d\n", buff, coef);
}

static ssize_t isg5320a_temp_enable_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;

	isg5320a_i2c_read(data, ISG5320A_TEMPERATURE_ENABLE_REG, &buff, 1);

	return snprintf(buf, PAGE_SIZE, "%d\n", ((buff & 0x40) >> 6));
}

static ssize_t isg5320a_irq_count_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	int ret = 0;
	s16 max_diff_val = 0;

	if (data->irq_count) {
		ret = -1;
		max_diff_val = data->max_diff;
	} else {
		max_diff_val = data->max_normal_diff;
	}

	GRIP_INFO("called\n");

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", ret, data->irq_count,
			max_diff_val);
}

static ssize_t isg5320a_irq_count_store(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		GRIP_ERR("invalid argument\n");
		return count;
	}

	mutex_lock(&data->lock);

	if (onoff == 0) {
		data->abnormal_mode = OFF;
	} else if (onoff == 1) {
		data->abnormal_mode = ON;
		data->irq_count = 0;
		data->max_diff = 0;
		data->max_normal_diff = 0;
	} else {
		GRIP_ERR("invalid value %d\n", onoff);
	}

	mutex_unlock(&data->lock);

	GRIP_INFO("%d\n", onoff);

	return count;
}

static ssize_t isg5320a_motion_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	if (data->motion)
		return snprintf(buf, PAGE_SIZE, "motion_detect\n");
	else
		return snprintf(buf, PAGE_SIZE, "motion_non_detect\n");
}

static ssize_t isg5320a_motion_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		GRIP_ERR("Invalid Argument\n");
		return ret;
	}

	if (val == 0) {
		GRIP_INFO("motion event off\n");
		data->motion = val;
	} else if (val == 1) {
		GRIP_INFO("motion event\n");
		data->motion = val;
	} else {
		GRIP_INFO("Invalid Argument : %u\n", val);
	}
	GRIP_INFO("%u\n", val);
	return count;
}

static ssize_t isg5320a_unknown_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		(data->is_unknown_mode == 1) ? "UNKNOWN" : "NORMAL");
}

static ssize_t isg5320a_unknown_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		GRIP_INFO("Invalid Argument\n");
		return ret;
	}

	if (val == 1)
		isg5320a_enter_unknown_mode(data, TYPE_FORCE);
	else if (val == 0)
		data->is_unknown_mode = UNKNOWN_OFF;
	else
		GRIP_INFO("Invalid Argument(%d)\n", val);

	GRIP_INFO("%u\n", val);
	return count;
}

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static ssize_t isg5320a_debug_raw_data_show(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u8 buff[10];
	u16 temp;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->lock);
	ret = isg5320a_i2c_read(data, ISG5320A_CDC16_A_H_REG, buff, sizeof(buff));
	mutex_unlock(&data->lock);
	if (ret < 0) {
		GRIP_INFO("fail to get data\n");
	} else {
		temp = ((u32)buff[0] << 8) | (u32)buff[1];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_cdc[0] = temp;

		temp = ((u32)buff[2] << 8) | (u32)buff[3];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_base[0] = temp;
		data->debug_diff[0] =
			(s32)data->debug_cdc[0] - (s32)data->debug_base[0];

		temp = ((u32)buff[6] << 8) | (u32)buff[7];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_cdc[1] = temp;

		temp = ((u32)buff[8] << 8) | (u32)buff[9];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_base[1] = temp;
		data->debug_diff[1] =
			(s32)data->debug_cdc[1] - (s32)data->debug_base[1];

		temp = ((u32)buff[4] << 8) | (u32)buff[5];
		if ((temp != 0) && (temp != 0xFFFF))
			data->debug_cdc[2] = temp;
		data->debug_diff[2] =
			(s32)data->debug_cdc[2] - (s32)data->debug_base[1];
	}

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", data->debug_cdc[0],
		       data->debug_diff[0], data->debug_base[0], data->debug_cdc[1],
		       data->debug_diff[1], data->debug_base[1], data->debug_cdc[2],
		       data->debug_diff[2], data->debug_base[1]);
}

static ssize_t isg5320a_debug_data_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d,%d,%d\n", data->cdc, data->base, data->diff);
}

static ssize_t isg5320a_reg_update_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int enable_backup;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	enable_backup = data->enable;

	isg5320a_reset(data);
	if (enable_backup)
		isg5320a_set_enable(data, OFF);
	isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);
	isg5320a_initialize(data);
	if (enable_backup)
		isg5320a_set_enable(data, ON);
	isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);

	return sprintf(buf, "OK\n");
}

#define DIRECT_CMD_WRITE            'w'
#define DIRECT_CMD_READ             'r'
#define DIRECT_BUF_COUNT            16
static ssize_t isg5320a_direct_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = -EPERM;
	u32 tmp1, tmp2;
	struct isg5320a_data *data = dev_get_drvdata(dev);
	direct_info *direct = (direct_info *)&data->direct;

	sscanf(buf, "%c %x %x", &direct->cmd, &tmp1, &tmp2);

	direct->addr = tmp1;
	direct->val = tmp2;

	GRIP_INFO("direct cmd: %c, addr: %x, val: %x\n", direct->cmd,
		direct->addr, direct->val);

	if ((direct->cmd != DIRECT_CMD_WRITE) && (direct->cmd != DIRECT_CMD_READ)) {
		GRIP_ERR("direct cmd is not correct!\n");
		return size;
	}

	if (direct->cmd == DIRECT_CMD_WRITE) {
		if (direct->addr == ISG5320A_IBAS_REG)
			data->debug_val[0] = direct->val;
		else if (direct->addr == ISG5320A_THD_REG)
			data->debug_val[1] = direct->val;
		else if (direct->addr == ISG5320A_TARGET_CDC_REG)
			data->debug_val[2] = direct->val;

		ret = isg5320a_i2c_write_one(data, direct->addr, direct->val);
		if (ret < 0)
			GRIP_ERR("direct write fail\n");
		else
			GRIP_INFO("direct write addr: %x, val: %x\n", direct->addr, direct->val);
	}

	return size;
}

static ssize_t isg5320a_direct_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int i, count = 0;
	int ret = 0;
	int len;
	u8 addr;
	const int msg_len = 256;
	char msg[256]; //msg_len
	struct isg5320a_data *data = dev_get_drvdata(dev);
	direct_info *direct = (direct_info *)&data->direct;
	u8 buf8[DIRECT_BUF_COUNT] = {0,};
	int max_len = DIRECT_BUF_COUNT;

	if (direct->cmd != DIRECT_CMD_READ)
		return sprintf(buf, "ex) echo r addr len size(display) > direct\n");

	len = direct->val;
	addr = direct->addr;

	while (len > 0) {
		if (len < max_len) max_len = len;

		ret = isg5320a_i2c_read(data, addr, buf8, max_len);
		if (ret < 0) {
			count = sprintf(buf, "i2c read fail\n");
			break;
		}
		addr += max_len;

		for (i = 0; i < max_len; i++) {
			count += snprintf(msg, msg_len, "0x%02X ", buf8[i]);
			strncat(buf, msg, msg_len);
		}
		count += snprintf(msg, msg_len, "\n");
		strncat(buf, msg, msg_len);

		len -= max_len;
	}

	return count;
}

static ssize_t isg5320a_intr_debug_store(struct device *dev,
					 struct device_attribute *attr, const char *buf, size_t size)
{
	u32 tmp1;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%x %d", &tmp1, &data->intr_debug_size);

	data->intr_debug_addr = tmp1;

	GRIP_INFO("intr debug addr: 0x%x, count: %d\n",
		data->intr_debug_addr, data->intr_debug_size);

	return size;
}

static ssize_t isg5320a_intr_debug_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	GRIP_INFO("intr debug addr: 0x%x, count: %d\n",
		data->intr_debug_addr, data->intr_debug_size);

	return sprintf(buf, "intr debug addr: 0x%x, count: %d\n",
		       data->intr_debug_addr, data->intr_debug_size);
}

static ssize_t isg5320a_cp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	u16 buff;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = isg5320a_i2c_read(data, ISG5320A_COARSE_B_REG, (u8 *)&buff, 2);
	if (ret < 0) {
		GRIP_INFO("fail to get cp\n");
	} else {
		data->fine_coarse = buff;
		GRIP_INFO("coarse B:%04X\n", data->fine_coarse);
	}

	return sprintf(buf, "%d%02d,0\n", (data->fine_coarse & 0xFF),
		       (data->fine_coarse >> 8) & 0x3F);
}

#define SCAN_INT            0x12
#define FAR_CLOSE_INT       0x0C
static ssize_t isg5320a_scan_int_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = isg5320a_i2c_write_one(data, ISG5320A_IRQCON_REG, SCAN_INT);
	if (ret < 0) {
		GRIP_ERR("fail to set scan done int\n");
		return sprintf(buf, "FAIL\n");
	} else {
		GRIP_INFO("set scan done int\n");
		return sprintf(buf, "OK\n");
	}
}

static ssize_t isg5320a_far_close_int_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	int ret;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = isg5320a_i2c_write_one(data, ISG5320A_IRQCON_REG, FAR_CLOSE_INT);
	if (ret < 0) {
		GRIP_ERR("fail to set normal int\n");
		return sprintf(buf, "FAIL\n");
	} else {
		GRIP_INFO("set normal int\n");
		return sprintf(buf, "OK\n");
	}
}

static ssize_t isg5320a_toggle_enable_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	int enable;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	enable = (data->enable == OFF) ? ON : OFF;
	isg5320a_set_enable(data, (int)enable);

	return sprintf(buf, "%d\n", data->enable);
}

static ssize_t isg5320a_cml_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);
	u8 buff;

	isg5320a_i2c_read(data, ISG5320A_CML_BIAS_REG, &buff, 1);

	return snprintf(buf, PAGE_SIZE, "%d\n", (buff & 0x07));
}

static ssize_t isg5320a_init_freq_test_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	data->freq_step = 1;
	data->freq_value = 0;

	return sprintf(buf, "OK\n");
}

static ssize_t isg5320a_change_freq_step_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	data->freq_step++;

	if (data->freq_step > ISG5320A_MAX_FREQ_STEP)
		data->freq_step = 1;

	return sprintf(buf, "%d\n", data->freq_step);
}

static ssize_t isg5320a_change_freq_value_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	data->freq_value += data->freq_step;

	return sprintf(buf, "%d\n", data->freq_value);
}

static ssize_t isg5320a_change_freq_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->lock);
	isg5320a_i2c_write_one(data, 0x38, 0x00);
	isg5320a_i2c_write_one(data, 0x19, 0x0A);
	usleep_range(10000,11000);

	isg5320a_i2c_write_one(data, ISG5320A_NUM_OF_CLK, data->freq_value);

	isg5320a_i2c_write_one(data, 0xCD, 0xDE);
	isg5320a_i2c_write_one(data, 0xC9, 0x10);
	usleep_range(10000,11000);

	isg5320a_i2c_write_one(data, 0xCD, 0xDE);
	isg5320a_i2c_write_one(data, 0xC9, 0x00);
	usleep_range(10000,11000);

	isg5320a_i2c_write_one(data, 0x19, 0x8A);
	isg5320a_i2c_write_one(data, 0x38, 0xDD);
	msleep(500);

	isg5320a_i2c_write_one(data, ISG5320A_SCANCTRL2_REG, ISG5320A_BFCAL_START);
	msleep(100);

	mutex_unlock(&data->lock);

	return sprintf(buf, "%d\n", 0);
}
#endif

static ssize_t isg5320a_noti_enable_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	u8 enable;
	struct isg5320a_data *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		GRIP_ERR("invalid argument\n");
		return size;
	}

	GRIP_INFO("new_value=%d\n", (int)enable);

	data->noti_enable = enable;

	if (data->noti_enable)
		isg5320a_enter_unknown_mode(data, TYPE_BOOT);

	return size;
}

static ssize_t isg5320a_noti_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	GRIP_INFO("noti_enable = %d\n", data->noti_enable);
	return sprintf(buf, "%d\n", data->noti_enable);
}

static DEVICE_ATTR(name, S_IRUGO, isg5320a_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, isg5320a_vendor_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, isg5320a_mode_show, NULL);
static DEVICE_ATTR(manual_acal, S_IRUGO, isg5320a_manual_acal_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO, isg5320a_acal_show, NULL);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
		   isg5320a_onoff_show, isg5320a_onoff_store);
static DEVICE_ATTR(reset, S_IRUGO, isg5320a_sw_reset_show, NULL);
static DEVICE_ATTR(normal_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		   isg5320a_normal_threshold_show, isg5320a_normal_threshold_store);
static DEVICE_ATTR(raw_data, S_IRUGO, isg5320a_raw_data_show, NULL);
static DEVICE_ATTR(diff_avg, S_IRUGO, isg5320a_diff_avg_show, NULL);
static DEVICE_ATTR(cdc_avg, S_IRUGO, isg5320a_cdc_avg_show, NULL);
static DEVICE_ATTR(useful_avg, S_IRUGO, isg5320a_cdc_avg_show, NULL);
static DEVICE_ATTR(ch_state, S_IRUGO, isg5320a_ch_state_show, NULL);
static DEVICE_ATTR(hysteresis, S_IRUGO, isg5320a_hysteresis_show, NULL);
static DEVICE_ATTR(sampling_freq, S_IRUGO, isg5320a_sampling_freq_show, NULL);
static DEVICE_ATTR(isum, S_IRUGO, isg5320a_isum_show, NULL);
static DEVICE_ATTR(scan_period, S_IRUGO, isg5320a_scan_period_show, NULL);
static DEVICE_ATTR(analog_gain, S_IRUGO, isg5320a_again_show, NULL);
static DEVICE_ATTR(cdc_up, S_IRUGO, isg5320a_cdc_down_coef_show, NULL);
static DEVICE_ATTR(cdc_down, S_IRUGO, isg5320a_cdc_up_coef_show, NULL);
static DEVICE_ATTR(temp_enable, S_IRUGO, isg5320a_temp_enable_show, NULL);
static DEVICE_ATTR(irq_count, S_IRUGO | S_IWUSR | S_IWGRP,
		   isg5320a_irq_count_show, isg5320a_irq_count_store);
static DEVICE_ATTR(motion, 0664, isg5320a_motion_show, isg5320a_motion_store);
static DEVICE_ATTR(unknown_state, 0664,
	isg5320a_unknown_state_show, isg5320a_unknown_state_store);
static DEVICE_ATTR(noti_enable, 0664, isg5320a_noti_enable_show, isg5320a_noti_enable_store);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static DEVICE_ATTR(debug_raw_data, S_IRUGO, isg5320a_debug_raw_data_show, NULL);
static DEVICE_ATTR(debug_data, S_IRUGO, isg5320a_debug_data_show, NULL);
static DEVICE_ATTR(reg_update, S_IRUGO, isg5320a_reg_update_show, NULL);
static DEVICE_ATTR(direct, S_IRUGO | S_IWUSR | S_IWGRP, isg5320a_direct_show,
		   isg5320a_direct_store);
static DEVICE_ATTR(intr_debug, S_IRUGO | S_IWUSR | S_IWGRP,
		   isg5320a_intr_debug_show, isg5320a_intr_debug_store);
static DEVICE_ATTR(cp, S_IRUGO, isg5320a_cp_show, NULL);
static DEVICE_ATTR(scan_int, S_IRUGO, isg5320a_scan_int_show, NULL);
static DEVICE_ATTR(far_close_int, S_IRUGO, isg5320a_far_close_int_show, NULL);
static DEVICE_ATTR(toggle_enable, S_IRUGO, isg5320a_toggle_enable_show, NULL);
static DEVICE_ATTR(cml, S_IRUGO, isg5320a_cml_show, NULL);
static DEVICE_ATTR(init_freq_test, S_IRUGO, isg5320a_init_freq_test_show, NULL);
static DEVICE_ATTR(change_freq_step, S_IRUGO, isg5320a_change_freq_step_show,
		NULL);
static DEVICE_ATTR(change_freq_value, S_IRUGO, isg5320a_change_freq_value_show,
		NULL);
static DEVICE_ATTR(change_freq, S_IRUGO, isg5320a_change_freq_show, NULL);
#endif

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_mode,
	&dev_attr_manual_acal,
	&dev_attr_calibration,
	&dev_attr_onoff,
	&dev_attr_reset,
	&dev_attr_normal_threshold,
	&dev_attr_raw_data,
	&dev_attr_diff_avg,
	&dev_attr_useful_avg,
	&dev_attr_cdc_avg,
	&dev_attr_ch_state,
	&dev_attr_hysteresis,
	&dev_attr_sampling_freq,
	&dev_attr_isum,
	&dev_attr_scan_period,
	&dev_attr_analog_gain,
	&dev_attr_cdc_up,
	&dev_attr_cdc_down,
	&dev_attr_temp_enable,
	&dev_attr_irq_count,
	&dev_attr_motion,
	&dev_attr_unknown_state,
	&dev_attr_noti_enable,
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	&dev_attr_debug_raw_data,
	&dev_attr_debug_data,
	&dev_attr_reg_update,
	&dev_attr_direct,
	&dev_attr_intr_debug,
	&dev_attr_cp,
	&dev_attr_scan_int,
	&dev_attr_far_close_int,
	&dev_attr_toggle_enable,
	&dev_attr_cml,
	&dev_attr_init_freq_test,
	&dev_attr_change_freq_step,
	&dev_attr_change_freq_value,
	&dev_attr_change_freq,
#endif
	NULL,
};

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   isg5320a_enable_show, isg5320a_enable_store);

static struct attribute *isg5320a_attributes[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group isg5320a_attribute_group = {
	.attrs = isg5320a_attributes,
};

#ifdef ISG5320A_INIT_DELAYEDWORK
static void init_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct isg5320a_data *data = container_of(delayed_work,
						  struct isg5320a_data, init_work);

	isg5320a_initialize(data);
	isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);
	isg5320a_set_debug_work(data, ON, 2000);
}
#endif

static void cfcal_work_func(struct work_struct *work)
{
	struct isg5320a_data *data = container_of((struct work_struct *)work,
						struct isg5320a_data, cfcal_work);

	isg5320a_force_calibration(data, false);
}

static void bfcal_work_func(struct work_struct *work)
{
	struct isg5320a_data *data = container_of((struct work_struct *)work,
						struct isg5320a_data, bfcal_work);

	isg5320a_force_calibration(data, true);
}

static void isg5320a_check_first_working(struct isg5320a_data *data)
{
	if (data->noti_enable && data->motion) {
		if (data->normal_th < data->diff) {
			if (!data->first_working) {
				data->first_working = true;
				GRIP_INFO("first working detected %d\n", data->diff);
			}
		} else {
			if (data->first_working &&
				(data->is_unknown_mode == UNKNOWN_ON)) {
				data->is_unknown_mode = UNKNOWN_OFF;
				GRIP_INFO("Release detected %d, unknown mode off\n", data->diff);
			}
		}
	}
}

static void cal_work_func(struct work_struct *work)
{

	struct delayed_work *delayed_work = to_delayed_work(work);
	struct isg5320a_data *data = container_of(delayed_work,
						  struct isg5320a_data, cal_work);
	bool force_cal = false;
	u8 buf[3];

	isg5320a_get_raw_data(data, false);
	if (data->is_unknown_mode == UNKNOWN_ON && data->motion)
		isg5320a_check_first_working(data);
	// check cfcal
	if ((data->cdc < data->cfcal_th) || !data->cal_done_flag || data->cdc_ret_flag) {
		//for check if registers need recovery
		isg5320a_i2c_read(data, ISG5320A_IBAS_REG, &buf[0], sizeof(buf[0]));
		isg5320a_i2c_read(data, ISG5320A_THD_REG, &buf[1], sizeof(buf[1]));
		isg5320a_i2c_read(data, ISG5320A_TARGET_CDC_REG, &buf[2], sizeof(buf[2]));

		if (buf[0] != data->debug_val[0] || buf[1] != data->debug_val[1] || buf[2] != data->debug_val[2]) {
			GRIP_ERR("reg check fail - reg[0] : %d %d, reg[1] : %d %d, reg[2] : %d %d\n",
				buf[0], data->debug_val[0], buf[1], data->debug_val[1], buf[2], data->debug_val[2]);

			isg5320a_reset(data);
			isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);
			isg5320a_initialize(data);
			isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);
			data->reg_err = true;
		}

		GRIP_INFO("cdc : %d, cfcal_th %d, cal_ok = %d cdc_fail = %d setup_parse_check = %d i2c_err = %d, reg_err = %d\n", data->cdc,
			data->cfcal_th, data->cal_done_flag, data->cdc_ret_flag, data->setup_reg_exist, data->i2c_fail_err, data->reg_err);
		isg5320a_force_calibration(data, false);
		isg5320a_enter_unknown_mode(data, TYPE_FORCE);
		force_cal = true;
	}
#if 0
	// check bfcal
	if (data->bfcal_chk_start) {
		data->bfcal_chk_count++;
		if (data->bfcal_chk_count == ISG5320A_BFCAL_CHK_RDY_TIME) {
			data->bfcal_chk_ready = true;
			data->bfcal_chk_cdc = data->cdc;
			data->bfcal_chk_diff =
				data->diff / ISG5320A_BFCAL_CHK_DIFF_RATIO;
		} else if (data->bfcal_chk_ready) {
			if (((data->bfcal_chk_count - ISG5320A_BFCAL_CHK_RDY_TIME) %
			     ISG5320A_BFCAL_CHK_CYCLE_TIME) == 0) {
				if (((s32)data->bfcal_chk_cdc - (s32)data->cdc) >=
				    data->bfcal_chk_diff) {
					isg5320a_force_calibration(data, true);
					isg5320a_enter_unknown_mode(data, TYPE_FORCE);
					force_cal = true;
					data->bfcal_chk_start = false;
					data->bfcal_chk_ready = false;
					data->bfcal_chk_count = 0;
				} else {
					data->bfcal_chk_cdc = data->cdc;
					data->bfcal_chk_diff =
						data->diff / ISG5320A_BFCAL_CHK_DIFF_RATIO;
				}
			}
		}
	}
#endif
	if (force_cal)
		schedule_delayed_work(&data->cal_work, msecs_to_jiffies(1000));
	else
		schedule_delayed_work(&data->cal_work, msecs_to_jiffies(500));
}

static void debug_work_func(struct work_struct *work)
{
	int ret;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct isg5320a_data *data = container_of(delayed_work,
						  struct isg5320a_data, debug_work);

	if (data->enable == ON) {
		if (data->abnormal_mode) {
			ret = isg5320a_get_raw_data(data, true);
			if (ret < 0) {
				GRIP_ERR("fail to read state(%d)\n", ret);
				return;
			}
			if (data->max_normal_diff < data->diff)
				data->max_normal_diff = data->diff;
		}
	}

	schedule_delayed_work(&data->debug_work, msecs_to_jiffies(2000));
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int isg5320a_pdic_handle_notification(struct notifier_block *nb,
					   unsigned long action, void *pdic_data)
{
	PD_NOTI_ATTACH_TYPEDEF usb_event = *(PD_NOTI_ATTACH_TYPEDEF *) pdic_data;
	struct isg5320a_data *data = container_of(nb, struct isg5320a_data, pdic_nb);

	GRIP_INFO("src %d id %d attach %d\n", usb_event.src, usb_event.id, usb_event.attach);
	if (!(usb_event.id == PDIC_NOTIFY_ID_ATTACH || usb_event.id == PDIC_NOTIFY_ID_OTG))
		return 0;

	if (data->pre_attach == usb_event.attach)
		return 0;

	if (data->initialized == ON) {
		if ((usb_event.attach == 0) || (usb_event.attach == 1)) {
			GRIP_INFO("accept attach = %d\n", (int)usb_event.attach);
			isg5320a_enter_unknown_mode(data, TYPE_USB);
			schedule_work(&data->cfcal_work);
		}
	}

	if (usb_event.rprd == PDIC_NOTIFY_HOST) {
		data->pre_otg_attach = usb_event.rprd;
		GRIP_INFO("otg attach");
	} else if (usb_event.id == PDIC_NOTIFY_ID_OTG) {
		data->pre_otg_attach = usb_event.attach;
		GRIP_INFO("otg attach");
	} else if (data->pre_otg_attach) {
		data->pre_otg_attach = 0;
		GRIP_INFO("otg detach");
	}
	data->pre_attach = usb_event.attach;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
static int isg5320a_hall_notifier(struct notifier_block *nb,
				unsigned long action, void *hall_data)
{
	struct hall_notifier_context *hall_notifier;
	struct isg5320a_data *data =
			container_of(nb, struct isg5320a_data, hall_nb);
	hall_notifier = hall_data;

	if (action == HALL_ATTACH) {
		GRIP_INFO("%s attach\n", hall_notifier->name);
		schedule_work(&data->bfcal_work);
	} else {
		GRIP_INFO("%s detach\n", hall_notifier->name);
	}

	isg5320a_enter_unknown_mode(data, TYPE_HALL);
	return 0;
}
#endif
static int isg5320a_parse_dt(struct isg5320a_data *data, struct device *dev)
{
	struct device_node *node = dev->of_node;
	enum of_gpio_flags flags;
	int ret;

	if (data->ic_num == MAIN_GRIP) {
		data->gpio_int = of_get_named_gpio_flags(node, "isg5320a,irq-gpio", 0,
							 &flags);
		if (data->gpio_int < 0) {
			GRIP_ERR("get gpio_int error\n");
			return -ENODEV;
		}

		GRIP_INFO("gpio_int:%d\n", data->gpio_int);

		ret = of_property_read_u32(node, "isg5320a,reg_num", &data->reg_size);
		if (ret < 0)
			data->reg_size = 150;

		ret = of_property_read_u8_array(node, "isg5320a,set_reg", data->setup_reg, data->reg_size * 2);
		if (ret < 0) {
			GRIP_ERR("set_reg fail\n");
			data->setup_reg_exist = false;
		} else {
			data->setup_reg_exist = true;
		}
	}
#if IS_ENABLED(CONFIG_SENSORS_ISG5320A_SUB)
	if (data->ic_num == SUB_GRIP) {
		data->gpio_int = of_get_named_gpio_flags(node, "isg5320a_sub,irq-gpio", 0,
							 &flags);
		if (data->gpio_int < 0) {
			GRIP_ERR("get gpio_int error\n");
			return -ENODEV;
		}

		GRIP_INFO("gpio_int:%d\n", data->gpio_int);

		ret = of_property_read_u32(node, "isg5320a_sub,reg_num", &data->reg_size);
		if (ret < 0)
			data->reg_size = 150;

		ret = of_property_read_u8_array(node, "isg5320a_sub,set_reg", data->setup_reg, data->reg_size * 2);
		if (ret < 0) {
			GRIP_ERR("set_reg fail\n");
			data->setup_reg_exist = false;
		} else {
			data->setup_reg_exist = true;
		}
	}
#endif
	return 0;
}

static int isg5320a_gpio_init(struct isg5320a_data *data)
{
	int ret = 0;

	ret = gpio_request(data->gpio_int, "isg5320a_irq");
	if (ret < 0) {
		GRIP_ERR("gpio %d request failed\n", data->gpio_int);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_int);
	if (ret < 0) {
		GRIP_ERR("failed to set gpio %d(%d)\n", data->gpio_int, ret);
		gpio_free(data->gpio_int);
		return ret;
	}

	return ret;
}

static int isg5320a_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct isg5320a_data *data;
	struct input_dev *input_dev;
	struct input_dev *noti_input_dev;
	int ic_num = 0;

	if (strcmp(client->name, "isg5320a") == 0)
		ic_num = MAIN_GRIP;
#if IS_ENABLED(CONFIG_SENSORS_ISG5320A_SUB)
	else if (strcmp(client->name, "isg5320a_sub") == 0)
		ic_num = SUB_GRIP;
#endif
	else {
		pr_err("[GRIP] client->name : %s, can't find grip ic num", client->name);
		return -1;
	}
	pr_info("[GRIP_%s] %s: start (0x%x)\n", isg5320a_grip_name[ic_num], __func__, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[GRIP] i2c_check_functionality error\n");
		goto err;
	}

	data = kzalloc(sizeof(struct isg5320a_data), GFP_KERNEL);
	if (!data) {
		pr_err("[GRIP] failed to allocate memory\n");
		goto err_kzalloc;
	}

	data->ic_num = ic_num;

	ret = isg5320a_parse_dt(data, &client->dev);
	if (ret) {
		GRIP_ERR("failed to parse dt\n");
		goto err_parse_dt;
	}

	ret = isg5320a_gpio_init(data);
	if (ret) {
		GRIP_ERR("failed to init sys\n");
		goto err_gpio_init;
	}

	data->client = client;
	i2c_set_clientdata(client, data);

	input_dev = input_allocate_device();
	if (!input_dev) {
		GRIP_ERR("input_allocate_device failed\n");
		goto err_input_alloc;
	}

	data->dev = &client->dev;
	data->input_dev = input_dev;

	input_dev->name = isg5320a_module_name[data->ic_num];
	input_dev->id.bustype = BUS_I2C;

	input_set_capability(input_dev, EV_REL, REL_MISC);
	input_set_capability(input_dev, EV_REL, REL_MAX);
	input_set_capability(input_dev, EV_REL, REL_X);
	input_set_drvdata(input_dev, data);

	noti_input_dev = input_allocate_device();
	if (!noti_input_dev) {
		pr_err("[GRIP_%d] input_allocate_device failed\n", data->ic_num);
		input_free_device(input_dev);
		goto err_noti_input_alloc;
	}

	data->dev = &client->dev;
	data->noti_input_dev = noti_input_dev;

	noti_input_dev->name = NOTI_MODULE_NAME;
	noti_input_dev->id.bustype = BUS_I2C;

	input_set_capability(noti_input_dev, EV_REL, REL_X);
	input_set_drvdata(noti_input_dev, data);

	ret = isg5320a_reset(data);
	if (ret < 0) {
		GRIP_ERR("IMAGIS reset failed\n");
		input_free_device(input_dev);
		input_free_device(noti_input_dev);
		goto err_soft_reset;
	}

	data->skip_data = false;
	data->state = FAR;
	data->enable = OFF;
	data->initialized = OFF;
	data->debug_cnt = 0;
	data->normal_th = 0;
	data->fine_coarse = 0;
	data->cfcal_th = ISG5320A_RESET_CONDITION;
	data->bfcal_chk_ready = false;
	data->bfcal_chk_start = false;
	data->bfcal_chk_count = 0;
	data->debug_cdc[0] = 0;
	data->debug_cdc[1] = 0;
	data->debug_cdc[2] = 0;
	data->debug_base[0] = 0;
	data->debug_base[1] = 0;
	data->debug_diff[0] = 0;
	data->debug_diff[1] = 0;
	data->cal_done_flag = 0;
	data->cdc_ret_flag = 0;
	data->i2c_fail_err = false;
	data->reg_err = false;
	data->is_unknown_mode = UNKNOWN_OFF;
	data->first_working = false;
	data->motion = 1;

	client->irq = gpio_to_irq(data->gpio_int);
	ret = request_threaded_irq(client->irq, NULL, isg5320a_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, isg5320a_device_name[data->ic_num], data);
	if (ret < 0) {
		GRIP_ERR("failed to register interrupt\n");
		input_free_device(input_dev);
		input_free_device(noti_input_dev);
		goto err_irq;
	}
	disable_irq(client->irq);
	mutex_init(&data->lock);

	ret = input_register_device(input_dev);
	if (ret) {
		GRIP_ERR("failed to register input dev (%d)\n", ret);
		input_free_device(input_dev);
		input_free_device(noti_input_dev);
		goto err_register_input_dev;
	}

	ret = input_register_device(noti_input_dev);
	if (ret) {
		GRIP_ERR("failed to register input dev for noti (%d)\n", ret);
		input_free_device(noti_input_dev);
		goto err_register_input_dev_noti;
	}

#if defined(CONFIG_SENSORS_CORE_AP)
	ret = sensors_create_symlink(&data->input_dev->dev.kobj,
				     data->input_dev->name);
	if (ret < 0) {
		GRIP_ERR("failed to create symlink (%d)\n", ret);
		goto err_create_symlink;
	}

	ret = sysfs_create_group(&data->input_dev->dev.kobj, &isg5320a_attribute_group);
	if (ret < 0) {
		GRIP_ERR("failed to create sysfs group (%d)\n", ret);
		goto err_sysfs_create_group;
	}

	ret = sensors_register(&data->dev, data, sensor_attrs, (char *)isg5320a_module_name[data->ic_num]);
	if (ret) {
		GRIP_ERR("could not register sensor(%d).\n", ret);
		goto err_sensor_register;
	}
#else
	ret = sensors_create_symlink(input_dev);
	if (ret < 0) {
		GRIP_ERR("failed to create symlink (%d)\n", ret);
		goto err_create_symlink;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj, &isg5320a_attribute_group);
	if (ret < 0) {
		GRIP_ERR("failed to create sysfs group (%d)\n", ret);
		goto err_sysfs_create_group;
	}

	ret = sensors_register(&data->dev, data, sensor_attrs, (char *)isg5320a_module_name[data->ic_num]);
	if (ret) {
		GRIP_ERR("could not register sensor(%d).\n", ret);
		goto err_sensor_register;
	}
#endif

	data->grip_ws = wakeup_source_register(&client->dev, "grip_wake_lock");
	INIT_DELAYED_WORK(&data->debug_work, debug_work_func);
	INIT_DELAYED_WORK(&data->cal_work, cal_work_func);
	INIT_WORK(&data->cfcal_work, cfcal_work_func);
	INIT_WORK(&data->bfcal_work, bfcal_work_func);
#ifdef ISG5320A_INIT_DELAYEDWORK
	INIT_DELAYED_WORK(&data->init_work, init_work_func);
	schedule_delayed_work(&data->init_work, msecs_to_jiffies(2000));
#else
	isg5320a_initialize(data);
	isg5320a_set_mode(data, ISG5320A_MODE_NORMAL);
	isg5320a_set_debug_work(data, ON, 2000);
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	data->pdic_status = OFF;
	data->pdic_pre_attach = 0;
	data->pre_otg_attach = 0;
	manager_notifier_register(&data->pdic_nb,
								isg5320a_pdic_handle_notification,
								MANAGER_NOTIFY_PDIC_SENSORHUB);
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
		GRIP_INFO("register hall notifier\n");
		data->hall_nb.priority = 1;
		data->hall_nb.notifier_call = isg5320a_hall_notifier;
		hall_notifier_register(&data->hall_nb);
#endif

	GRIP_INFO("### IMAGIS probe done ###\n");

	return 0;

err_sensor_register:
	sysfs_remove_group(&input_dev->dev.kobj, &isg5320a_attribute_group);
err_sysfs_create_group:
#if defined(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input_dev->dev.kobj, data->input_dev->name);
#else
	sensors_remove_symlink(input_dev);
#endif
err_create_symlink:
	input_unregister_device(noti_input_dev);
err_register_input_dev_noti:
	input_unregister_device(input_dev);
err_register_input_dev:
	mutex_destroy(&data->lock);
	free_irq(client->irq, data);
err_irq:
err_soft_reset:
err_input_alloc:
err_noti_input_alloc:
	gpio_free(data->gpio_int);
err_gpio_init:
err_parse_dt:
	kfree(data);
err_kzalloc:
err:
	pr_err("### IMAGIS probe failed ###\n");

	return -ENODEV;
}

static int isg5320a_remove(struct i2c_client *client)
{
	struct isg5320a_data *data = i2c_get_clientdata(client);

	GRIP_INFO("\n");

	isg5320a_set_debug_work(data, OFF, 0);
	if (data->enable == ON)
		isg5320a_set_enable(data, OFF);
	isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);

	free_irq(client->irq, data);
	gpio_free(data->gpio_int);

	wakeup_source_unregister(data->grip_ws);
	sensors_unregister(data->dev, sensor_attrs);
#if defined(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input_dev->dev.kobj, data->input_dev->name);
#else
	sensors_remove_symlink(data->input_dev);
#endif
	sysfs_remove_group(&data->input_dev->dev.kobj, &isg5320a_attribute_group);
	input_unregister_device(data->input_dev);
	mutex_destroy(&data->lock);

	kfree(data);

	return 0;
}

static int isg5320a_suspend(struct device *dev)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	GRIP_INFO("current_state : %d\n", data->enable);
	isg5320a_set_debug_work(data, OFF, 0);

	cancel_work_sync(&data->cfcal_work);
	cancel_work_sync(&data->bfcal_work);

	if (data->enable)
		disable_irq(data->client->irq);

	return 0;
}

static int isg5320a_resume(struct device *dev)
{
	struct isg5320a_data *data = dev_get_drvdata(dev);

	GRIP_INFO("current_state : %d\n", data->enable);
	isg5320a_set_debug_work(data, ON, 1000);

	if (data->enable)
		enable_irq(data->client->irq);

	return 0;
}

static void isg5320a_shutdown(struct i2c_client *client)
{
	struct isg5320a_data *data = i2c_get_clientdata(client);

	GRIP_INFO("\n");

	isg5320a_set_debug_work(data, OFF, 0);
	if (data->enable == ON)
		isg5320a_set_enable(data, OFF);
	isg5320a_set_mode(data, ISG5320A_MODE_SLEEP);
}

static const struct of_device_id isg5320a_match_table[] = {
	{ .compatible = "isg5320a", },
	{ },
};

static struct i2c_device_id isg5320a_id_table[] = {
	{ "ISG5320A", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, isg5320a_id_table);

static const struct dev_pm_ops isg5320a_pm_ops = {
	.suspend = isg5320a_suspend,
	.resume = isg5320a_resume,
};

static struct i2c_driver isg5320a_driver = {
	.driver = {
		.name = "ISG5320A",
		.owner = THIS_MODULE,
		.of_match_table = isg5320a_match_table,
		.pm = &isg5320a_pm_ops,
	},
	.id_table = isg5320a_id_table,
	.probe = isg5320a_probe,
	.remove = isg5320a_remove,
	.shutdown = isg5320a_shutdown,
};

#if IS_ENABLED(CONFIG_SENSORS_ISG5320A_SUB)
static const struct of_device_id isg5320a_sub_match_table[] = {
	{ .compatible = "isg5320a_sub", },
	{ },
};

static struct i2c_device_id isg5320a_sub_id_table[] = {
	{ "ISG5320A_SUB", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, isg5320a_sub_id_table);

static struct i2c_driver isg5320a_sub_driver = {
	.driver = {
		.name = "ISG5320A_SUB",
		.owner = THIS_MODULE,
		.of_match_table = isg5320a_sub_match_table,
		.pm = &isg5320a_pm_ops,
	},
	.id_table = isg5320a_sub_id_table,
	.probe = isg5320a_probe,
	.remove = isg5320a_remove,
	.shutdown = isg5320a_shutdown,
};
#endif
static int __init isg5320a_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&isg5320a_driver);
	if (ret != 0)
		pr_err("[GRIP] isg5320a_driver probe fail\n");
#if IS_ENABLED(CONFIG_SENSORS_ISG5320A_SUB)
	ret = i2c_add_driver(&isg5320a_sub_driver);
	if (ret != 0)
		pr_err("[GRIP_SUB] isg5320a_sub_driver probe fail\n");
#endif
	return ret;
}

static void __exit isg5320a_exit(void)
{
	i2c_del_driver(&isg5320a_driver);
}

module_init(isg5320a_init);
module_exit(isg5320a_exit);

MODULE_DESCRIPTION("Imagis Grip Sensor driver");
MODULE_LICENSE("GPL");
