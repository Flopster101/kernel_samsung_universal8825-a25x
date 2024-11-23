/*
 * Driver for the SX938x
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include "sx938x.h"

#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
#include <linux/sensor/sensors_core.h>
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
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



#define I2C_M_WR                 0 /* for i2c Write */
//#define I2C_M_RD                 1 /* for i2c Read */

#define IDLE                     0
#define ACTIVE                   1

#define DIFF_READ_NUM            10
#define GRIP_LOG_TIME            15 /* 30 sec */

#define REF_OFF_MAIN_ON			0x02
#define REF_OFF_MAIN_OFF		0x00

#define IRQ_PROCESS_CONDITION   (SX938x_IRQSTAT_TOUCH_FLAG	\
				| SX938x_IRQSTAT_RELEASE_FLAG	\
				| SX938x_IRQSTAT_COMPDONE_FLAG)

#define SX938x_MODE_SLEEP        0
#define SX938x_MODE_NORMAL       1


#define IDLE			0
#define ACTIVE			1

/* Failure Index */
#define SX938x_ID_ERROR		(-1)
#define SX938x_NIRQ_ERROR	(-2)
#define SX938x_CONN_ERROR	(-3)
#define SX938x_I2C_ERROR	(-4)
#define SX938x_REG_ERROR	(-5)

#define GRIP_WORKING 1
#define GRIP_RELEASE 2

#define INTERRUPT_HIGH 1
#define INTERRUPT_LOW 0

#define TYPE_USB   1
#define TYPE_HALL  2
#define TYPE_BOOT  3
#define TYPE_FORCE 4

#define UNKNOWN_ON  1
#define UNKNOWN_OFF 2

struct sx938x_p {
	struct i2c_client *client;
	struct input_dev *input;
	struct input_dev *noti_input_dev;
	struct device *factory_device;
	struct delayed_work init_work;
	struct delayed_work irq_work;
	struct delayed_work debug_work;
	struct wakeup_source *grip_ws;
	struct mutex mode_mutex;
	struct mutex read_mutex;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct notifier_block pdic_nb;
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	struct notifier_block hall_nb;
#endif
	struct regulator *dvdd_vreg;	/* regulator */
	const char *dvdd_vreg_name;	/* regulator name */

#if IS_ENABLED(CONFIG_SENSORS_COMMON_VDD_SUB)
	int gpio_nirq_sub;
#endif
	int pre_attach;
	int debug_count;
	int debug_zero_count;
	int fail_status_code;
	int is_unknown_mode;
	int motion;
	int irq_count;
	int abnormal_mode;
	int ldo_en;
	int irq;
	int gpio_nirq;
	int state;
	int init_done;
	int noti_enable;
	int again_m;
	int dgain_m;
	int diff_cnt;

	atomic_t enable;

	u32 unknown_sel;

	s32 useful_avg;
	s32 capMain;
	s32 useful;

	u16 detect_threshold;
	u16 offset;

	s16 avg;
	s16 diff;
	s16 diff_avg;
	s16 max_diff;
	s16 max_normal_diff;

	u8 ic_num;

	bool first_working;
	bool skip_data;
};

static int sx938x_get_nirq_state(struct sx938x_p *data)
{
	return gpio_get_value_cansleep(data->gpio_nirq);
}


static int sx938x_i2c_write(struct sx938x_p *data, u8 reg_addr, u8 buf)
{
	int ret;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	msg.addr = data->client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret < 0) {
		GRIP_ERR("err %d", ret);
		data->fail_status_code = SX938x_I2C_ERROR;
	}
	return 0;
}

static int sx938x_i2c_read(struct sx938x_p *data, u8 reg_addr, u8 *buf)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = data->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(data->client->adapter, msg, 2);
	if (ret < 0) {
		GRIP_ERR("err %d", ret);
		data->fail_status_code = SX938x_I2C_ERROR;
	}

	return ret;
}

static int sx938x_read_reg_stat(struct sx938x_p *data)
{
	u8 value = 0;

	if (data) {
		if (sx938x_i2c_read(data, SX938x_IRQSTAT_REG, &value) >= 0)
			return (value & 0x00FF);
	}
	return 0;
}

static void sx938x_initialize_register(struct sx938x_p *data)
{
	u8 val = 0;
	int idx;

	data->init_done = OFF;

	for (idx = 0; idx < (int)(ARRAY_SIZE(setup_reg[data->ic_num])); idx++) {
		sx938x_i2c_write(data, setup_reg[data->ic_num][idx].reg, setup_reg[data->ic_num][idx].val);
		GRIP_INFO("Write Reg 0x%x Val 0x%x\n",
			setup_reg[data->ic_num][idx].reg,
			setup_reg[data->ic_num][idx].val);

		sx938x_i2c_read(data, setup_reg[data->ic_num][idx].reg, &val);
		GRIP_INFO("Read Reg 0x%x Val 0x%x\n",
			setup_reg[data->ic_num][idx].reg, val);
	}

	val = 0;
	sx938x_i2c_read(data, SX938x_PROXCTRL5_REG, &val);
	data->detect_threshold = (u16)val * (u16)val / 2;

	val = 0;
	sx938x_i2c_read(data, SX938x_PROXCTRL4_REG, &val);
	val = (val & 0x30) >> 4;

	if (val)
		data->detect_threshold += data->detect_threshold >> (5 - val);

	GRIP_INFO("detect threshold %u\n", data->detect_threshold);

	data->init_done = ON;
}

static int sx938x_hardware_check(struct sx938x_p *data)
{
	int ret;
	u8 whoami = 0;
	u8 loop = 0;

	//Check th IRQ Status
	GRIP_INFO("irq state %d", sx938x_get_nirq_state(data));

	while (sx938x_get_nirq_state(data) == INTERRUPT_LOW) {
			sx938x_read_reg_stat(data);
			GRIP_INFO("irq state %d", sx938x_get_nirq_state(data));
			if (++loop > 10) {
				data->fail_status_code = SX938x_NIRQ_ERROR;
				return data->fail_status_code;
			}
			usleep_range(10000, 11000);
	}
	ret = sx938x_i2c_read(data, SX938x_WHOAMI_REG, &whoami);
	if (ret < 0) {
		data->fail_status_code = SX938x_I2C_ERROR;
		return data->fail_status_code;
	}

	GRIP_INFO("whoami 0x%x\n", whoami);

	if (whoami != SX938x_WHOAMI_VALUE) {
		data->fail_status_code = SX938x_ID_ERROR;
		return data->fail_status_code;
	}

	return ret;
}

static int sx938x_manual_offset_calibration(struct sx938x_p *data)
{
	s32 ret = 0;

	GRIP_INFO("\n");
	ret = sx938x_i2c_write(data, SX938x_STAT_REG, SX938x_STAT_COMPSTAT_ALL_FLAG);
	return ret;
}


static void sx938x_send_event(struct sx938x_p *data, u8 state)
{
	if (data->skip_data == true) {
		GRIP_INFO("skip grip event\n");
		return;
	}

	if (state == ACTIVE) {
		data->state = ACTIVE;
		GRIP_INFO("touched\n");
	} else {
		data->state = IDLE;
		GRIP_INFO("released\n");
	}

	if (state == ACTIVE)
		input_report_rel(data->input, REL_MISC, GRIP_WORKING);
	else
		input_report_rel(data->input, REL_MISC, GRIP_RELEASE);

	if (data->unknown_sel)
		input_report_rel(data->input, REL_X, data->is_unknown_mode);
	input_sync(data->input);
}

static void sx938x_get_gain(struct sx938x_p *data)
{
	u8 msByte = 0;
	static const int again_phm[] = {7500, 22500, 37500, 52500, 60000, 75000, 90000, 105000};

	sx938x_i2c_read(data, SX938x_AFE_PARAM1_PHM_REG, &msByte);
	msByte = (msByte >> 4) & 0x07;
	data->again_m = again_phm[msByte];

	msByte = 0;
	sx938x_i2c_read(data, SX938x_PROXCTRL0_PHM_REG, &msByte);
	msByte = (msByte >> 3) & 0x07;
	if (msByte)
		data->dgain_m = 1 << (msByte - 1);
	else
		data->dgain_m = 1;
}

static void sx938x_get_data(struct sx938x_p *data)
{
	u8 msb = 0, lsb = 0;
	u8 convstat = 0;
	s16 useful_ref, retry = 0;
	u16 offset_ref = 0;
	int diff = 0;

	mutex_lock(&data->read_mutex);
	if (data) {
		/* Semtech reference code receives the DIFF data, when 'CONVERSION' is completed (IRQ falling),
		but Samsung receive diff data by POLLING. So we have to check the completion of 'CONVERSION'.*/
		while (1) {
			sx938x_i2c_read(data, SX938x_STAT_REG, &convstat);
			convstat &= 0x01;

			if (++retry > 5 || convstat == 0)
				break;

			usleep_range(10000, 11000);
		}
		GRIP_INFO("retry %d, CONVSTAT %u\n", retry, convstat);
		//READ REF Channel data
		sx938x_i2c_read(data, SX938x_USEMSB_PHR, &msb);
		sx938x_i2c_read(data, SX938x_USELSB_PHR, &lsb);
		useful_ref = (s16)((msb << 8) | lsb);

		msb = lsb = 0;
		sx938x_i2c_read(data, SX938x_OFFSETMSB_PHR, &msb);
		sx938x_i2c_read(data, SX938x_OFFSETLSB_PHR, &lsb);
		offset_ref = (u16)((msb << 8) | lsb);

		//READ Main channel data
		msb = lsb = 0;
		sx938x_i2c_read(data, SX938x_USEMSB_PHM, &msb);
		sx938x_i2c_read(data, SX938x_USELSB_PHM, &lsb);
		data->useful = (s16)((msb << 8) | lsb);

		msb = lsb = 0;
		sx938x_i2c_read(data, SX938x_AVGMSB_PHM, &msb);
		sx938x_i2c_read(data, SX938x_AVGLSB_PHM, &lsb);
		data->avg = (s16)((msb << 8) | lsb);

#if 0
		sx938x_i2c_read(data, SX938x_DIFFMSB_PHM, &msb);
		sx938x_i2c_read(data, SX938x_DIFFLSB_PHM, &lsb);
		data->diff = (s32)((msb << 8) | lsb);
		if (data->diff > 32767)
			data->diff -= 65536;
#endif
		diff = data->useful - data->avg;
		if (diff > 32767)
			data->diff = 32767;
		else if (diff < -32768)
			data->diff = -32768;
		else
			data->diff = diff;

		msb = lsb = 0;
		sx938x_i2c_read(data, SX938x_OFFSETMSB_PHM, &msb);
		sx938x_i2c_read(data, SX938x_OFFSETLSB_PHM, &lsb);
		data->capMain = (int)(msb >> 7) * 2369640 + (int)(msb & 0x7F) * 30380 + (int)(lsb * 270) +
						(int)(((int)data->useful * data->again_m) / (data->dgain_m * 32768));
		data->offset = (u16)((msb << 8) | lsb);

		GRIP_INFO("[MAIN] capMain %d Useful %d Average %d DIFF %d Offset %d [REF] Useful %d Offset %d\n",
					data->capMain, data->useful, data->avg, data->diff,
					data->offset, useful_ref, offset_ref);
	}
	mutex_unlock(&data->read_mutex);
}

static int sx938x_set_mode(struct sx938x_p *data, unsigned char mode)
{
	int ret = -EINVAL;
	u8 val = 0;

	GRIP_INFO("%u\n", mode);

	mutex_lock(&data->mode_mutex);
	if (mode == SX938x_MODE_SLEEP) {
		ret = sx938x_i2c_write(data, SX938x_GNRL_CTRL0_REG, REF_OFF_MAIN_OFF);
	} else if (mode == SX938x_MODE_NORMAL) {
		val = setup_reg[data->ic_num][SX938x_GNRL_CTRL0_REG_IDX].val;
		ret = sx938x_i2c_write(data, SX938x_GNRL_CTRL0_REG, val);
		msleep(20);
		sx938x_manual_offset_calibration(data);
		msleep(450);
	}

	GRIP_INFO("changed %u\n", mode);

	mutex_unlock(&data->mode_mutex);
	return ret;
}

static void sx938x_check_status(struct sx938x_p *data)
{
	u8 status = 0;

	sx938x_i2c_read(data, SX938x_STAT_REG, &status);

	GRIP_INFO("0x%x\n", status);

	if (data->skip_data == true) {
		input_report_rel(data->input, REL_MISC, GRIP_RELEASE);
		if (data->unknown_sel)
			input_report_rel(data->input, REL_X, UNKNOWN_OFF);
		input_sync(data->input);
		return;
	}

	if ((status & SX938x_PROXSTAT_FLAG) && (data->diff > data->detect_threshold))
		sx938x_send_event(data, ACTIVE);
	else
		sx938x_send_event(data, IDLE);
}

static void sx938x_set_enable(struct sx938x_p *data, int enable)
{
	int pre_enable = atomic_read(&data->enable);

	GRIP_INFO("%d\n", enable);

	if (enable) {
		if (pre_enable == OFF) {
			data->diff_avg = 0;
			data->diff_cnt = 0;
			data->useful_avg = 0;
			sx938x_get_data(data);
			sx938x_check_status(data);

			msleep(20);
			sx938x_read_reg_stat(data);

			/* enable interrupt */
			sx938x_i2c_write(data, SX938x_IRQ_ENABLE_REG, 0x0E);

			enable_irq(data->irq);
			enable_irq_wake(data->irq);

			atomic_set(&data->enable, ON);
		}
	} else {
		if (pre_enable == ON) {
			/* disable interrupt */
			sx938x_i2c_write(data, SX938x_IRQ_ENABLE_REG, 0x00);

			disable_irq(data->irq);
			disable_irq_wake(data->irq);

			atomic_set(&data->enable, OFF);
		}
	}
}

static void sx938x_set_debug_work(struct sx938x_p *data, u8 enable,
				  unsigned int time_ms)
{
	if (enable == ON) {
		data->debug_count = 0;
		data->debug_zero_count = 0;
		schedule_delayed_work(&data->debug_work,
				      msecs_to_jiffies(time_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
	}
}

static void sx938x_enter_unknown_mode(struct sx938x_p *data, int type)
{
	if (data->noti_enable && !data->skip_data && data->unknown_sel) {
		data->motion = 0;
		data->first_working = false;
		if (data->is_unknown_mode == UNKNOWN_OFF) {
			data->is_unknown_mode = UNKNOWN_ON;
			if (atomic_read(&data->enable) == ON) {
				input_report_rel(data->input, REL_X, UNKNOWN_ON);
				input_sync(data->input);
			}
			GRIP_INFO("UNKNOWN Re-enter\n");
		} else {
			GRIP_INFO("already UNKNOWN\n");
		}
		input_report_rel(data->noti_input_dev, REL_X, type);
		input_sync(data->noti_input_dev);
	}
}

static ssize_t sx938x_get_offset_calibration_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	struct sx938x_p *data = dev_get_drvdata(dev);

	sx938x_i2c_read(data, SX938x_IRQSTAT_REG, &reg_value);

	return sprintf(buf, "%d\n", reg_value);
}

static ssize_t sx938x_set_offset_calibration_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	if (val)
		sx938x_manual_offset_calibration(data);

	return count;
}

static ssize_t sx938x_sw_reset_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 compstat = 0xFF;
	int retry = 0;

	GRIP_INFO("\n");
	sx938x_manual_offset_calibration(data);
	msleep(450);
	sx938x_get_data(data);

	while (retry++ <= 3) {
		sx938x_i2c_read(data, SX938x_STAT_REG, &compstat);
		GRIP_INFO("compstat 0x%x\n", compstat);
		compstat &= 0x04;

		if (compstat == 0)
			break;

		msleep(50);
	}

	if (compstat == 0)
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	return snprintf(buf, PAGE_SIZE, "%d\n", -1);
}

static ssize_t sx938x_vendor_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx938x_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", device_name[data->ic_num]);
}

static ssize_t sx938x_touch_mode_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static ssize_t sx938x_raw_data_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	static s32 sum_diff, sum_useful;
	struct sx938x_p *data = dev_get_drvdata(dev);

	sx938x_get_data(data);

	if (data->diff_cnt == 0) {
		sum_diff = (s32)data->diff;
		sum_useful = data->useful;
	} else {
		sum_diff += (s32)data->diff;
		sum_useful += data->useful;
	}

	if (++data->diff_cnt >= DIFF_READ_NUM) {
		data->diff_avg = (s16)(sum_diff / DIFF_READ_NUM);
		data->useful_avg = sum_useful / DIFF_READ_NUM;
		data->diff_cnt = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%ld,%ld,%u,%d,%d\n", (long int)data->capMain,
			(long int)data->useful, data->offset, data->diff, data->avg);
}

static ssize_t sx938x_diff_avg_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->diff_avg);
}

static ssize_t sx938x_useful_avg_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%ld\n", (long int)data->useful_avg);
}

static ssize_t sx938x_avgnegfilt_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 avgnegfilt = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL3_REG, &avgnegfilt);

	avgnegfilt = (avgnegfilt & 0x38) >> 3;

	if (avgnegfilt == 7)
		return snprintf(buf, PAGE_SIZE, "1\n");
	else if (avgnegfilt > 0 && avgnegfilt < 7)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << avgnegfilt);
	else if (avgnegfilt == 0)
		return snprintf(buf, PAGE_SIZE, "0\n");

	return snprintf(buf, PAGE_SIZE, "not set\n");
}

static ssize_t sx938x_avgposfilt_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 avgposfilt = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL3_REG, &avgposfilt);
	avgposfilt = avgposfilt & 0x07;

	if (avgposfilt == 7)
		return snprintf(buf, PAGE_SIZE, "1\n");
	else if (avgposfilt > 1 && avgposfilt < 7)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 16 << avgposfilt);
	else if (avgposfilt == 1)
		return snprintf(buf, PAGE_SIZE, "1-1/16\n");
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t sx938x_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 gain = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL0_PHM_REG, &gain);
	gain = (gain & 0x38) >> 3;

	if (gain > 0 && gain < 5)
		return snprintf(buf, PAGE_SIZE, "x%u\n", 1 << (gain - 1));

	return snprintf(buf, PAGE_SIZE, "Reserved\n");
}

static ssize_t sx938x_avgthresh_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 avgthresh = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL1_REG, &avgthresh);
	avgthresh = avgthresh & 0x3F;

	return snprintf(buf, PAGE_SIZE, "%ld\n", 512 * (long int)avgthresh);
}

static ssize_t sx938x_rawfilt_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 rawfilt = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL0_PHM_REG, &rawfilt);
	rawfilt = rawfilt & 0x07;

	if (rawfilt > 0 && rawfilt < 8)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << rawfilt);
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t sx938x_sampling_freq_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 sampling_freq = 0;
	const char *table[16] = {
		"250", "200", "166.67", "142.86", "125", "100",	"83.33", "71.43",
		"62.50", "50", "41.67", "35.71", "27.78", "20.83", "15.62", "7.81"
	};

	sx938x_i2c_read(data, SX938x_AFE_PARAM1_PHM_REG, &sampling_freq);
	sampling_freq = sampling_freq & 0x0F;

	return snprintf(buf, PAGE_SIZE, "%skHz\n", table[sampling_freq]);
}

static ssize_t sx938x_scan_period_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 scan_period = 0;

	sx938x_i2c_read(data, SX938x_GNRL_CTRL2_REG, &scan_period);

	return snprintf(buf, PAGE_SIZE, "%ld\n",
			(long int)(((long int)scan_period << 11) / 1000));
}

static ssize_t sx938x_again_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	const char *table[8] = {
		"+/-0.75", "+/-2.25", "+/-3.75", "+/-5.25",
		"+/-6", "+/-7.5", "+/-9", "+/-10.5"
	};
	u8 again = 0;

	sx938x_i2c_read(data, SX938x_AFE_PARAM1_PHM_REG, &again);
	again = (again & 0x70) >> 4;

	return snprintf(buf, PAGE_SIZE, "%spF\n", table[again]);
}

static ssize_t sx938x_phase_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static ssize_t sx938x_hysteresis_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	const char *table[4] = {"None", "+/-6%", "+/-12%", "+/-25%"};
	u8 hyst = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL4_REG, &hyst);
	hyst = (hyst & 0x30) >> 4;

	return snprintf(buf, PAGE_SIZE, "%s\n", table[hyst]);
}

static ssize_t sx938x_resolution_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 resolution = 0;

	sx938x_i2c_read(data, SX938x_AFE_PARAM0_PHM_REG, &resolution);
	resolution = resolution & 0x7;

	return snprintf(buf, PAGE_SIZE, "%u\n", 1 << (resolution + 3));
}

static ssize_t sx938x_useful_filt_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 useful_filt = 0;

	sx938x_i2c_read(data, SX938x_USEFILTER4_REG, &useful_filt);
	useful_filt = useful_filt & 0x01;

	return snprintf(buf, PAGE_SIZE, "%s\n", useful_filt ? "on" : "off");
}

static ssize_t sx938x_resistor_filter_input_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 resistor_filter = 0;

	sx938x_i2c_read(data, SX938x_AFE_CTRL1_REG, &resistor_filter);
	resistor_filter = resistor_filter & 0x0F;

	return snprintf(buf, PAGE_SIZE, "%d kohm\n", resistor_filter * 2);
}

static ssize_t sx938x_closedeb_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 closedeb = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL4_REG, &closedeb);
	closedeb = closedeb & 0x0C;
	closedeb = closedeb >> 2;

	if (closedeb == 0)
		return snprintf(buf, PAGE_SIZE, "off");
	return snprintf(buf, PAGE_SIZE, "%d\n", 1 << closedeb);
}

static ssize_t sx938x_fardeb_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 fardeb = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL4_REG, &fardeb);
	fardeb  = fardeb  & 0x03;

	if (fardeb == 0)
		return snprintf(buf, PAGE_SIZE, "off");
	return snprintf(buf, PAGE_SIZE, "%d\n", 1 << fardeb);
}


static ssize_t sx938x_irq_count_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	int ret = 0;
	s16 max_diff_val = 0;

	if (data->irq_count) {
		ret = -1;
		max_diff_val = data->max_diff;
	} else {
		max_diff_val = data->max_normal_diff;
	}

	GRIP_INFO("called\n");

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			ret, data->irq_count, max_diff_val);
}

static ssize_t sx938x_irq_count_store(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t count)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return count;
	}

	mutex_lock(&data->read_mutex);

	if (onoff == 0) {
		data->abnormal_mode = OFF;
	} else if (onoff == 1) {
		data->abnormal_mode = ON;
		data->irq_count = 0;
		data->max_diff = 0;
		data->max_normal_diff = 0;
	} else {
		GRIP_ERR("unknown val %d\n", onoff);
	}

	mutex_unlock(&data->read_mutex);

	GRIP_INFO("%d\n", onoff);

	return count;
}

static ssize_t sx938x_normal_threshold_show(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	u8 th_buf = 0, hyst = 0;
	u32 threshold = 0;

	sx938x_i2c_read(data, SX938x_PROXCTRL5_REG, &th_buf);
	threshold = (u32)th_buf * (u32)th_buf / 2;

	sx938x_i2c_read(data, SX938x_PROXCTRL4_REG, &hyst);
	hyst = (hyst & 0x30) >> 4;

	switch (hyst) {
	case 0x01: /* 6% */
		hyst = threshold >> 4;
		break;
	case 0x02: /* 12% */
		hyst = threshold >> 3;
		break;
	case 0x03: /* 25% */
		hyst = threshold >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%lu,%lu\n",
			(u32)threshold + (u32)hyst, (u32)threshold - (u32)hyst);
}

static ssize_t sx938x_onoff_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->skip_data);
}

static ssize_t sx938x_onoff_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx938x_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	if (val == 0) {
		data->skip_data = true;
		if (atomic_read(&data->enable) == ON) {
			data->state = IDLE;
			input_report_rel(data->input, REL_MISC, GRIP_RELEASE);
			if (data->unknown_sel)
				input_report_rel(data->input, REL_X, UNKNOWN_OFF);
			input_sync(data->input);
		}
		data->motion = 1;
		data->is_unknown_mode = UNKNOWN_OFF;
		data->first_working = false;
	} else {
		data->skip_data = false;
	}

	GRIP_INFO("%u\n", val);
	return count;
}

static ssize_t sx938x_register_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int idx;
	int i = 0;
	u8 val;
	char *p = buf;
	struct sx938x_p *data = dev_get_drvdata(dev);

	for (idx = 0; idx < (int)(ARRAY_SIZE(setup_reg[data->ic_num])); idx++) {
		sx938x_i2c_read(data, setup_reg[data->ic_num][idx].reg, &val);
		i += snprintf(p + i, PAGE_SIZE - i, "(0x%02x)=0x%02x\n", setup_reg[data->ic_num][idx].reg, val);
	}

	return i;
}


static ssize_t sx938x_register_write_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int reg_address = 0, val = 0;
	struct sx938x_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
		GRIP_ERR("sscanf err\n");
		return -EINVAL;
	}

	sx938x_i2c_write(data, (unsigned char)reg_address, (unsigned char)val);
	GRIP_INFO("Regi 0x%x Val 0x%x\n", reg_address, val);

	sx938x_get_gain(data);

	return count;
}

static unsigned int register_read_store_reg;
static unsigned int register_read_store_val;
static ssize_t sx938x_register_read_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%02x \n", register_read_store_val);
}

static ssize_t sx938x_register_read_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val = 0;
	int regist = 0;
	struct sx938x_p *data = dev_get_drvdata(dev);

	GRIP_INFO("\n");

	if (sscanf(buf, "%x", &regist) != 1) {
		GRIP_ERR("sscanf err\n");
		return -EINVAL;
	}

	sx938x_i2c_read(data, regist, &val);
	GRIP_INFO("Regi 0x%2x Val 0x%2x\n", regist, val);
	register_read_store_reg = (unsigned int)regist;
	register_read_store_val = (unsigned int)val;
	return count;
}

/* show far near status reg data */
static ssize_t sx938x_status_regdata_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val0;
	struct sx938x_p *data = dev_get_drvdata(dev);

	sx938x_i2c_read(data, SX938x_STAT_REG, &val0);
	GRIP_INFO("Status 0x%2x \n", val0);

	return sprintf(buf, "%d \n", val0);
}

static ssize_t sx938x_status_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	struct sx938x_p *data = dev_get_drvdata(dev);

	sx938x_i2c_read(data, SX938x_STAT_REG, &val);

	if ((val & 0x08) == 0)//bit3 for proxstat
		status = 0;
	else
		status = 1;

	return sprintf(buf, "%d\n", status);
}

/* check if manual calibrate success or not */
static ssize_t sx938x_cal_state_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	struct sx938x_p *data = dev_get_drvdata(dev);

	sx938x_i2c_read(data, SX938x_STAT_REG, &val);
	GRIP_INFO("Regi 0x01 Val 0x%2x\n", val);
	if ((val & 0x06) == 0)
		status = 1;
	else
		status = 0;

	return sprintf(buf, "%d\n", status);
}

static ssize_t sx938x_motion_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		data->motion == 1 ? "motion_detect" : "motion_non_detect");
}

static ssize_t sx938x_motion_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx938x_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	data->motion = val;

	GRIP_INFO("%u\n", val);
	return count;
}

static ssize_t sx938x_unknown_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		(data->is_unknown_mode == UNKNOWN_ON) ?	"UNKNOWN" : "NORMAL");
}

static ssize_t sx938x_unknown_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx938x_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	if (val == 1)
		sx938x_enter_unknown_mode(data, TYPE_FORCE);
	else if (val == 0)
		data->is_unknown_mode = UNKNOWN_OFF;
	else
		GRIP_INFO("Invalid Val %u\n", val);

	GRIP_INFO("%u\n", val);

	return count;
}

static ssize_t sx938x_noti_enable_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	u8 enable;
	struct sx938x_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return size;
	}

	GRIP_INFO("new val %d\n", (int)enable);

	data->noti_enable = enable;

	if (data->noti_enable)
		sx938x_enter_unknown_mode(data, TYPE_BOOT);
	else {
		data->motion = 1;
		data->first_working = false;
		data->is_unknown_mode = UNKNOWN_OFF;
	}

	return size;
}

static ssize_t sx938x_noti_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->noti_enable);
}

static DEVICE_ATTR(regproxdata, 0444, sx938x_status_regdata_show, NULL);
static DEVICE_ATTR(proxstatus, 0444, sx938x_status_show, NULL);
static DEVICE_ATTR(cal_state, 0444, sx938x_cal_state_show, NULL);

static DEVICE_ATTR(menual_calibrate, S_IRUGO | S_IWUSR | S_IWGRP,
		   sx938x_get_offset_calibration_show,
		   sx938x_set_offset_calibration_store);
static DEVICE_ATTR(register_write, S_IWUSR | S_IWGRP,
		   NULL, sx938x_register_write_store);
static DEVICE_ATTR(register_read_all, S_IRUGO, sx938x_register_show, NULL);
static DEVICE_ATTR(register_read, S_IRUGO | S_IWUSR | S_IWGRP,
			sx938x_register_read_show, sx938x_register_read_store);
static DEVICE_ATTR(reset, S_IRUGO, sx938x_sw_reset_show, NULL);

static DEVICE_ATTR(name, S_IRUGO, sx938x_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx938x_vendor_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, sx938x_touch_mode_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, sx938x_raw_data_show, NULL);
static DEVICE_ATTR(diff_avg, S_IRUGO, sx938x_diff_avg_show, NULL);
static DEVICE_ATTR(useful_avg, S_IRUGO, sx938x_useful_avg_show, NULL);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
		   sx938x_onoff_show, sx938x_onoff_store);
static DEVICE_ATTR(normal_threshold, S_IRUGO,
		   sx938x_normal_threshold_show, NULL);

static DEVICE_ATTR(avg_negfilt, S_IRUGO, sx938x_avgnegfilt_show, NULL);
static DEVICE_ATTR(avg_posfilt, S_IRUGO, sx938x_avgposfilt_show, NULL);
static DEVICE_ATTR(avg_thresh, S_IRUGO, sx938x_avgthresh_show, NULL);
static DEVICE_ATTR(rawfilt, S_IRUGO, sx938x_rawfilt_show, NULL);
static DEVICE_ATTR(sampling_freq, S_IRUGO, sx938x_sampling_freq_show, NULL);
static DEVICE_ATTR(scan_period, S_IRUGO, sx938x_scan_period_show, NULL);
static DEVICE_ATTR(gain, S_IRUGO, sx938x_gain_show, NULL);
static DEVICE_ATTR(analog_gain, S_IRUGO, sx938x_again_show, NULL);
static DEVICE_ATTR(phase, S_IRUGO, sx938x_phase_show, NULL);
static DEVICE_ATTR(hysteresis, S_IRUGO, sx938x_hysteresis_show, NULL);
static DEVICE_ATTR(irq_count, S_IRUGO | S_IWUSR | S_IWGRP,
		   sx938x_irq_count_show, sx938x_irq_count_store);
static DEVICE_ATTR(resolution, S_IRUGO, sx938x_resolution_show, NULL);
static DEVICE_ATTR(useful_filt, S_IRUGO, sx938x_useful_filt_show, NULL);
static DEVICE_ATTR(resistor_filter_input, S_IRUGO, sx938x_resistor_filter_input_show, NULL);
static DEVICE_ATTR(closedeb, S_IRUGO, sx938x_closedeb_show, NULL);
static DEVICE_ATTR(fardeb, S_IRUGO, sx938x_fardeb_show, NULL);
static DEVICE_ATTR(motion, S_IRUGO | S_IWUSR | S_IWGRP,
	sx938x_motion_show, sx938x_motion_store);
static DEVICE_ATTR(unknown_state, S_IRUGO | S_IWUSR | S_IWGRP,
	sx938x_unknown_state_show, sx938x_unknown_state_store);
static DEVICE_ATTR(noti_enable, 0664, sx938x_noti_enable_show, sx938x_noti_enable_store);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_regproxdata,
	&dev_attr_proxstatus,
	&dev_attr_cal_state,
	&dev_attr_menual_calibrate,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_register_read_all,
	&dev_attr_reset,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_mode,
	&dev_attr_raw_data,
	&dev_attr_diff_avg,
	&dev_attr_useful_avg,
	&dev_attr_onoff,
	&dev_attr_normal_threshold,
	&dev_attr_avg_negfilt,
	&dev_attr_avg_posfilt,
	&dev_attr_avg_thresh,
	&dev_attr_rawfilt,
	&dev_attr_sampling_freq,
	&dev_attr_scan_period,
	&dev_attr_gain,
	&dev_attr_analog_gain,
	&dev_attr_phase,
	&dev_attr_hysteresis,
	&dev_attr_irq_count,
	&dev_attr_resolution,
	&dev_attr_useful_filt,
	&dev_attr_resistor_filter_input,
	&dev_attr_closedeb,
	&dev_attr_fardeb,
	&dev_attr_motion,
	&dev_attr_unknown_state,
	&dev_attr_noti_enable,
	NULL,
};

static ssize_t sx938x_enable_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static ssize_t sx938x_enable_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx938x_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	GRIP_INFO("new val %u\n", enable);
	if ((enable == 0) || (enable == 1))
		sx938x_set_enable(data, (int)enable);
	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   sx938x_enable_show, sx938x_enable_store);

static struct attribute *sx938x_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group sx938x_attribute_group = {
	.attrs = sx938x_attributes
};

static void sx938x_touch_process(struct sx938x_p *data)
{
	u8 status = 0;

	sx938x_i2c_read(data, SX938x_STAT_REG, &status);
	GRIP_INFO("0x%x\n", status);

	sx938x_get_data(data);

	if (data->abnormal_mode) {
		if (status & SX938x_PROXSTAT_FLAG) {
			if (data->max_diff < data->diff)
				data->max_diff = data->diff;
			data->irq_count++;
		}
	}

	if (data->state == IDLE) {
		if (status & SX938x_PROXSTAT_FLAG) {
			if (data->is_unknown_mode == UNKNOWN_ON && data->motion)
				data->first_working = true;
			sx938x_send_event(data, ACTIVE);
		} else {
			GRIP_INFO("0x%x already released.\n", status);
		}
	} else { /* User released button */
		if (!(status & SX938x_PROXSTAT_FLAG)) {
			if (data->is_unknown_mode == UNKNOWN_ON && data->motion) {
				GRIP_INFO("unknown mode off\n");
				data->is_unknown_mode = UNKNOWN_OFF;
			}
			sx938x_send_event(data, IDLE);
		} else {
			GRIP_INFO("0x%x still touched\n", status);
		}
	}
}

static void sx938x_process_interrupt(struct sx938x_p *data)
{
	u8 status = 0;

	/* since we are not in an interrupt don't need to disable irq. */
	status = sx938x_read_reg_stat(data);

	GRIP_INFO("status %d\n", status);

	if (status & IRQ_PROCESS_CONDITION)
		sx938x_touch_process(data);
}

static void sx938x_init_work_func(struct work_struct *work)
{
	struct sx938x_p *data = container_of((struct delayed_work *)work,
					     struct sx938x_p, init_work);

	if (data) {
		GRIP_INFO("initialize\n");

		sx938x_initialize_register(data);
		msleep(100);
		sx938x_manual_offset_calibration(data);

		sx938x_set_mode(data, SX938x_MODE_NORMAL);

		sx938x_read_reg_stat(data);
		sx938x_get_gain(data);
	}
	return;
}

static void sx938x_irq_work_func(struct work_struct *work)
{
	struct sx938x_p *data = container_of((struct delayed_work *)work,
					     struct sx938x_p, irq_work);

	if (sx938x_get_nirq_state(data) == INTERRUPT_LOW)
		sx938x_process_interrupt(data);
	else
		GRIP_ERR("nirq read high %d\n", sx938x_get_nirq_state(data));
}

static void sx938x_check_first_working(struct sx938x_p *data)
{
	if (data->noti_enable && data->motion) {
		if (data->detect_threshold < data->diff) {
			data->first_working = true;
			GRIP_INFO("first working detected %d\n", data->diff);
		} else {
			if (data->first_working) {
				data->is_unknown_mode = UNKNOWN_OFF;
				GRIP_INFO("Release detected %d, unknown mode off\n", data->diff);
			}
		}
	}
}

static void sx938x_debug_work_func(struct work_struct *work)
{
	struct sx938x_p *data = container_of((struct delayed_work *)work,
					     struct sx938x_p, debug_work);

	if (atomic_read(&data->enable) == ON) {
		if (data->abnormal_mode) {
			sx938x_get_data(data);
			if (data->max_normal_diff < data->diff)
				data->max_normal_diff = data->diff;
		}
	}

	if (data->debug_count >= GRIP_LOG_TIME) {
		if (data->fail_status_code != 0)
			GRIP_ERR("err code %d", data->fail_status_code);
		sx938x_get_data(data);
		if (data->is_unknown_mode == UNKNOWN_ON && data->motion)
			sx938x_check_first_working(data);
		data->debug_count = 0;
	} else {
		if (data->is_unknown_mode == UNKNOWN_ON && data->motion) {
			sx938x_get_data(data);
			sx938x_check_first_working(data);
		}
		data->debug_count++;
	}

	schedule_delayed_work(&data->debug_work, msecs_to_jiffies(2000));
}

static irqreturn_t sx938x_interrupt_thread(int irq, void *pdata)
{
	struct sx938x_p *data = pdata;

	__pm_wakeup_event(data->grip_ws, jiffies_to_msecs(3 * HZ));
	schedule_delayed_work(&data->irq_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static int sx938x_input_init(struct sx938x_p *data)
{
	int ret = 0;
	struct input_dev *dev = NULL;

	/* Create the input device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name =  module_name[data->ic_num];
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_MISC);
	input_set_capability(dev, EV_REL, REL_X);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		GRIP_ERR("fail to create symlink %d\n", ret);
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &sx938x_attribute_group);
	if (ret < 0) {
		GRIP_ERR("fail to create sysfs group %d\n", ret);
		sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
		input_unregister_device(dev);
		return ret;
	}
#else
	ret = sensors_create_symlink(dev);
	if (ret < 0) {
		GRIP_ERR("fail to create symlink %d\n", ret);
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &sx938x_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(dev);
		input_unregister_device(dev);
		return ret;
	}
#endif
	/* save the input pointer and finish initialization */
	data->input = dev;

	return 0;
}

static int sx938x_noti_input_init(struct sx938x_p *data)
{
	int ret = 0;
	struct input_dev *noti_input_dev = NULL;

	if (data->unknown_sel) {
		/* Create the input device */
		noti_input_dev = input_allocate_device();
		if (!noti_input_dev) {
			GRIP_ERR("input_allocate_device fail\n");
			return -ENOMEM;
		}

		noti_input_dev->name = NOTI_MODULE_NAME;
		noti_input_dev->id.bustype = BUS_I2C;

		input_set_capability(noti_input_dev, EV_REL, REL_X);
		input_set_drvdata(noti_input_dev, data);

		ret = input_register_device(noti_input_dev);
		if (ret < 0) {
			GRIP_ERR("fail to regi input dev for noti %d\n", ret);
			input_free_device(noti_input_dev);
			return ret;
		}

		data->noti_input_dev = noti_input_dev;
	}

	return 0;
}

static int sx938x_setup_pin(struct sx938x_p *data)
{
	int ret;

	ret = gpio_request(data->gpio_nirq, "SX938X_nIRQ");
	if (ret < 0) {
		GRIP_ERR("gpio %d req fail %d\n", data->gpio_nirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_nirq);
	if (ret < 0) {
		GRIP_ERR("fail to set gpio %d as input %d\n", data->gpio_nirq, ret);
		gpio_free(data->gpio_nirq);
		return ret;
	}

	return 0;

}

/* There is an issue that a grip address depends on the sequence of power and IRQ. So, Add power control function */
static int sx938x_power_onoff(struct sx938x_p *data, bool on)
{
	int ret = 0;
	int voltage = 0;
	int reg_enabled = 0;

	if (data->ldo_en) {
		ret = gpio_request(data->ldo_en, "sx938x_ldo_en");
		if (ret < 0) {
			GRIP_ERR("gpio %d req fail %d\n", data->ldo_en, ret);
			return ret;
		}
		gpio_set_value(data->ldo_en, on);
		GRIP_INFO("ldo_en power %d\n", on);
		gpio_free(data->ldo_en);
	}

	if (data->dvdd_vreg_name) {
		if (data->dvdd_vreg == NULL) {
			data->dvdd_vreg = regulator_get(NULL, data->dvdd_vreg_name);
			if (IS_ERR(data->dvdd_vreg)) {
				data->dvdd_vreg = NULL;
				GRIP_ERR("fail to get dvdd_vreg %s\n", data->dvdd_vreg_name);
			}
		}
	}

	if (data->dvdd_vreg) {
		voltage = regulator_get_voltage(data->dvdd_vreg);
		reg_enabled = regulator_is_enabled(data->dvdd_vreg);
		GRIP_INFO("dvdd_vreg reg_enabled=%d voltage=%d\n", reg_enabled, voltage);
	}

	if (on) {
		if (data->dvdd_vreg) {
			if (reg_enabled == 0) {
				ret = regulator_enable(data->dvdd_vreg);
				if (ret) {
					GRIP_ERR("dvdd reg enable fail\n");
					return ret;
				}
				GRIP_INFO("dvdd_vreg turn on\n");
			}
		}
	} else {
		if (data->dvdd_vreg) {
			if (reg_enabled == 1) {
				ret = regulator_disable(data->dvdd_vreg);
				if (ret) {
					GRIP_ERR("dvdd reg disable fail\n");
					return ret;
				}
				GRIP_INFO("dvdd_vreg turn off\n");
			}
		}
	}
	GRIP_INFO("%s\n", on ? "on" : "off");

	return ret;
}


static void sx938x_initialize_variable(struct sx938x_p *data)
{
	data->init_done = OFF;
	data->skip_data = false;
	data->state = IDLE;
	data->fail_status_code = 0;
	data->pre_attach = -1;

	data->is_unknown_mode = UNKNOWN_OFF;
	data->motion = 1;
	data->first_working = false;

	atomic_set(&data->enable, OFF);
}


static int sx938x_read_setupreg(struct sx938x_p *data, struct device_node *dnode, const char *str, int idx)
{
	u32 temp_val;
	int ret;

	ret = of_property_read_u32(dnode, str, &temp_val);
	if (!ret) {
		setup_reg[data->ic_num][idx].val = (u8)temp_val;
		GRIP_INFO("reg debug str %s add 0x%2x val 0x%2x", 
				str, setup_reg[data->ic_num][idx].reg,
				setup_reg[data->ic_num][idx].val);
	} else if (ret == -22)
		GRIP_ERR("%s default 0x%2x %d\n", str, temp_val, ret);
	else {
		GRIP_ERR("%s property read err 0x%2x %d\n", str, temp_val, ret);
		data->fail_status_code = SX938x_REG_ERROR;
	}
	return ret;
}

static int sx938x_check_dependency(struct device *dev, int ic_num)
{
	struct device_node *dNode = dev->of_node;
	struct regulator *dvdd_vreg = NULL;	/* regulator */
	char *dvdd_vreg_name = NULL;	/* regulator name */

	if (ic_num == MAIN_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x,dvdd_vreg_name", 0,
				(const char **)&dvdd_vreg_name)) {
			dvdd_vreg_name = NULL;
		}
	}
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	if (ic_num == SUB_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x_sub,dvdd_vreg_name", 0,
						(const char **)&dvdd_vreg_name)) {
			dvdd_vreg_name = NULL;
		}
	}
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	if (ic_num == SUB2_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x_sub2,dvdd_vreg_name", 0,
						(const char **)&dvdd_vreg_name)) {
			dvdd_vreg_name = NULL;
		}
	}
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	if (ic_num == WIFI_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x_wifi,dvdd_vreg_name", 0,
						(const char **)&dvdd_vreg_name)) {
			dvdd_vreg_name = NULL;
		}
	}
#endif
	pr_info("[GRIP_%d] dvdd_vreg_name %s\n", ic_num, dvdd_vreg_name);

	if (dvdd_vreg_name) {
		if (dvdd_vreg == NULL) {
			dvdd_vreg = regulator_get(NULL, dvdd_vreg_name);
			if (IS_ERR(dvdd_vreg)) {
				pr_info("[GRIP_%d] %s\n", ic_num, __func__);
				return -EPROBE_DEFER;
			}
		}
	}

	return 0;
}

static int sx938x_parse_dt(struct sx938x_p *data, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;
	int reg_size = 0, ret = 0;
	int i;

	if (dNode == NULL)
		return -ENODEV;

	if (data->ic_num == MAIN_GRIP) {
#if IS_ENABLED(CONFIG_SENSORS_COMMON_VDD_SUB)
		data->gpio_nirq_sub = of_get_named_gpio_flags(dNode,
							  "sx938x,nirq_gpio_sub", 0, &flags);
		if (data->gpio_nirq_sub < 0)
			GRIP_ERR("nirq_gpio_sub is null\n");
		else
			GRIP_INFO("get nirq_gpio_sub %d\n", data->gpio_nirq_sub);
#endif
		data->gpio_nirq = of_get_named_gpio_flags(dNode,
							  "sx938x,nirq-gpio", 0, &flags);
		ret = of_property_read_u32(dNode, "sx938x,unknown_sel", &data->unknown_sel);
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	} else if (data->ic_num == SUB_GRIP) {
		data->gpio_nirq = of_get_named_gpio_flags(dNode, "sx938x_sub,nirq-gpio", 0, &flags);
		ret = of_property_read_u32(dNode, "sx938x_sub,unknown_sel", &data->unknown_sel);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	} else if (data->ic_num == SUB2_GRIP) {
		data->gpio_nirq = of_get_named_gpio_flags(dNode, "sx938x_sub2,nirq-gpio", 0, &flags);
		ret = of_property_read_u32(dNode, "sx938x_sub2,unknown_sel", &data->unknown_sel);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	} else if (data->ic_num == WIFI_GRIP) {
		data->gpio_nirq = of_get_named_gpio_flags(dNode, "sx938x_wifi,nirq-gpio", 0, &flags);
		ret = of_property_read_u32(dNode, "sx938x_wifi,unknown_sel", &data->unknown_sel);
#endif
	}

	if (ret < 0) {
		GRIP_ERR("unknown_sel read fail %d\n", ret);
		data->unknown_sel = 1;
		ret = 0;
	}

	GRIP_INFO("unknown_sel %d\n", data->unknown_sel);

	if (data->gpio_nirq < 0) {
		GRIP_ERR("get gpio_nirq err\n");
		return -ENODEV;
	} else {
		GRIP_INFO("get gpio_nirq %d\n", data->gpio_nirq);
	}

	if (data->ic_num == MAIN_GRIP)
		data->ldo_en = of_get_named_gpio_flags(dNode, "sx938x,ldo_en", 0, &flags);
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	if (data->ic_num == SUB_GRIP)
		data->ldo_en = of_get_named_gpio_flags(dNode, "sx938x_sub,ldo_en", 0, &flags);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	if (data->ic_num == SUB2_GRIP)
		data->ldo_en = of_get_named_gpio_flags(dNode, "sx938x_sub2,ldo_en", 0, &flags);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	if (data->ic_num == WIFI_GRIP)
		data->ldo_en = of_get_named_gpio_flags(dNode, "sx938x_wifi,ldo_en", 0, &flags);
#endif

	if (data->ldo_en < 0) {
		GRIP_ERR("skip ldo_en\n");
		data->ldo_en = 0;
	} else {
		if (data->ic_num == MAIN_GRIP)
			ret = gpio_request(data->ldo_en, "sx938x_ldo_en");
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
		if (data->ic_num == SUB_GRIP)
			ret = gpio_request(data->ldo_en, "sx938x_sub_ldo_en");
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
		if (data->ic_num == SUB2_GRIP)
			ret = gpio_request(data->ldo_en, "sx938x_sub2_ldo_en");
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
		if (data->ic_num == WIFI_GRIP)
			ret = gpio_request(data->ldo_en, "sx938x_wifi_ldo_en");
#endif
		if (ret < 0) {
			GRIP_ERR("gpio %d req fail %d\n", data->ldo_en, ret);
			return ret;
		}
		gpio_direction_output(data->ldo_en, 0);
		gpio_free(data->ldo_en);
	}

	if (data->ic_num == MAIN_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x,dvdd_vreg_name", 0,
				(const char **)&data->dvdd_vreg_name)) {
			data->dvdd_vreg_name = NULL;
		}
	}
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	if (data->ic_num == SUB_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x_sub,dvdd_vreg_name", 0,
						(const char **)&data->dvdd_vreg_name)) {
			data->dvdd_vreg_name = NULL;
		}
	}
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	if (data->ic_num == SUB2_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x_sub2,dvdd_vreg_name", 0,
						(const char **)&data->dvdd_vreg_name)) {
			data->dvdd_vreg_name = NULL;
		}
	}
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	if (data->ic_num == WIFI_GRIP) {
		if (of_property_read_string_index(dNode, "sx938x_wifi,dvdd_vreg_name", 0,
						(const char **)&data->dvdd_vreg_name)) {
			data->dvdd_vreg_name = NULL;
		}
	}
#endif
		GRIP_INFO("dvdd_vreg_name %s\n", data->dvdd_vreg_name);

	if (data->ic_num == MAIN_GRIP)
		reg_size = sizeof(sx938x_parse_reg) / sizeof(char *);
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	else if (data->ic_num == SUB_GRIP)
		reg_size = sizeof(sx938x_sub_parse_reg) / sizeof(char *);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	else if (data->ic_num == SUB2_GRIP)
		reg_size = sizeof(sx938x_sub2_parse_reg) / sizeof(char *);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	else if (data->ic_num == WIFI_GRIP)
		reg_size = sizeof(sx938x_wifi_parse_reg) / sizeof(char *);
#endif
	for (i = 0; i < reg_size; i++) {
		if (data->ic_num == MAIN_GRIP)
			sx938x_read_setupreg(data, dNode, sx938x_parse_reg[i], i);
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
		else if (data->ic_num == SUB_GRIP)
			sx938x_read_setupreg(data, dNode, sx938x_sub_parse_reg[i], i);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
		else if (data->ic_num == SUB2_GRIP)
			sx938x_read_setupreg(data, dNode, sx938x_sub2_parse_reg[i], i);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
		else if (data->ic_num == WIFI_GRIP)
			sx938x_read_setupreg(data, dNode, sx938x_wifi_parse_reg[i], i);
#endif
	}

	return 0;
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int sx938x_pdic_handle_notification(struct notifier_block *nb,
					   unsigned long action, void *pdic_data)
{
	PD_NOTI_ATTACH_TYPEDEF usb_typec_info = *(PD_NOTI_ATTACH_TYPEDEF *)pdic_data;
	struct sx938x_p *data = container_of(nb, struct sx938x_p, pdic_nb);

	if (usb_typec_info.id != PDIC_NOTIFY_ID_ATTACH)
		return 0;

	if (data->pre_attach == usb_typec_info.attach)
		return 0;

	GRIP_INFO("src %d id %d attach %d rprd %d\n",
		usb_typec_info.src, usb_typec_info.id, usb_typec_info.attach, usb_typec_info.rprd);

	if (data->init_done == ON) {
			sx938x_enter_unknown_mode(data, TYPE_USB);
			sx938x_manual_offset_calibration(data);
	}

	data->pre_attach = usb_typec_info.attach;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
static int sx938x_hall_notifier(struct notifier_block *nb,
				unsigned long action, void *hall_data)
{
	struct hall_notifier_context *hall_notifier;
	struct sx938x_p *data =
			container_of(nb, struct sx938x_p, hall_nb);
	hall_notifier = hall_data;

	if (action == HALL_ATTACH) {
		GRIP_INFO("%s attach\n", hall_notifier->name);
		sx938x_enter_unknown_mode(data, TYPE_HALL);
		sx938x_manual_offset_calibration(data);
	} else {
		sx938x_enter_unknown_mode(data, TYPE_HALL);
		return 0;
	}

	return 0;
}
#endif

static int sx938x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct sx938x_p *data = NULL;
	int ic_num = 0;

	if (strcmp(client->name, "sx938x") == 0)
		ic_num = MAIN_GRIP;
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	else if (strcmp(client->name, "sx938x_sub") == 0)
		ic_num = SUB_GRIP;
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	else if (strcmp(client->name, "sx938x_sub2") == 0)
		ic_num = SUB2_GRIP;
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	else if (strcmp(client->name, "sx938x_wifi") == 0)
		ic_num = WIFI_GRIP;
#endif
	else {
		pr_err("[GRIP] name %s can't find grip ic num", client->name);
		return -1;
	}
	pr_info("[GRIP_%s] %s start 0x%x\n", grip_name[ic_num], __func__, client->addr);

	ret = sx938x_check_dependency(&client->dev, ic_num);
	if (ret < 0)
		return ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("[GRIP_%s] i2c func err\n", grip_name[ic_num]);
		goto exit;
	}

	data = kzalloc(sizeof(struct sx938x_p), GFP_KERNEL);
	if (data == NULL) {
		pr_info("[GRIP_%s] Fail to mem alloc\n", grip_name[ic_num]);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	data->ic_num = ic_num;
	i2c_set_clientdata(client, data);
	data->client = client;
	data->factory_device = &client->dev;

	ret = sx938x_input_init(data);
	if (ret < 0)
		goto exit_input_init;

	data->grip_ws = wakeup_source_register(&client->dev, "grip_wake_lock");
	mutex_init(&data->mode_mutex);
	mutex_init(&data->read_mutex);

	ret = sx938x_parse_dt(data, &client->dev);
	if (ret < 0) {
		GRIP_ERR("of_node err\n");
		ret = -ENODEV;
		goto exit_of_node;
	}
	ret = sx938x_noti_input_init(data);
	if (ret < 0)
		goto exit_noti_input_init;

//If Host NIRQ pin has a protection diode and PULL UP (state HIGH), addr can be 0x29 or 0x2A
//However, if the state of the pin is LOW, 0x28 is guaranteed.
#if IS_ENABLED(CONFIG_SENSORS_COMMON_VDD_SUB)
	if (ic_num == SUB_GRIP) {
		GRIP_INFO("skip irq outmode\n");
	} else {
		ret = gpio_direction_output(data->gpio_nirq, 0);
		if (ret < 0)
			GRIP_ERR("could not setup outmode\n");
		if (ic_num == MAIN_GRIP) {
			ret = gpio_direction_output(data->gpio_nirq_sub, 0);
			if (ret < 0)
				GRIP_ERR("could not setup outmode(sub)\n");
		}
		usleep_range(1000, 1100);
		sx938x_power_onoff(data, 1);
		usleep_range(6000, 6100); // Tpor > 5ms
	}
#else
	ret = gpio_direction_output(data->gpio_nirq, 0);
	if (ret < 0)
		GRIP_ERR("could not setup outmode\n");
	usleep_range(1000, 1100);

	sx938x_power_onoff(data, 1);
	usleep_range(6000, 6100); // Tpor > 5ms
#endif

	ret = sx938x_setup_pin(data);
	if (ret) {
		GRIP_ERR("could not setup pin\n");
		goto exit_setup_pin;
	}

	/* read chip id */
	ret = sx938x_hardware_check(data);
	if (ret < 0) {
		GRIP_ERR("chip id check fail %d\n", ret);
		goto exit_chip_reset;
	}

	sx938x_initialize_variable(data);
	INIT_DELAYED_WORK(&data->init_work, sx938x_init_work_func);
	INIT_DELAYED_WORK(&data->irq_work, sx938x_irq_work_func);
	INIT_DELAYED_WORK(&data->debug_work, sx938x_debug_work_func);

	data->irq = gpio_to_irq(data->gpio_nirq);
	/* initailize interrupt reporting */
	ret = request_threaded_irq(data->irq, NULL, sx938x_interrupt_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				    device_name[data->ic_num], data);
	if (ret < 0) {
		GRIP_ERR("fail to req thread irq %d ret %d\n", data->irq, ret);
		goto exit_request_threaded_irq;
	}
	disable_irq(data->irq);

	ret = sensors_register(&data->factory_device, data, sensor_attrs, (char *)module_name[data->ic_num]);
	if (ret) {
		GRIP_ERR("could not regi sensor %d\n", ret);
		goto exit_register_failed;
	}

	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));
	sx938x_set_debug_work(data, ON, 20000);

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	GRIP_INFO("regi pdic notifier\n");
	manager_notifier_register(&data->pdic_nb,
				  sx938x_pdic_handle_notification,
				  MANAGER_NOTIFY_PDIC_SENSORHUB);
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
		GRIP_INFO("regi hall notifier\n");
		data->hall_nb.priority = 1;
		data->hall_nb.notifier_call = sx938x_hall_notifier;
		hall_notifier_register(&data->hall_nb);
#endif

	GRIP_INFO("done!\n");

	return 0;
exit_register_failed:
	sensors_unregister(data->factory_device, sensor_attrs);
	free_irq(data->irq, data);
exit_request_threaded_irq:
exit_chip_reset:
	gpio_free(data->gpio_nirq);
exit_setup_pin:
	if (data->unknown_sel)
		input_unregister_device(data->noti_input_dev);
exit_noti_input_init:
exit_of_node:
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->read_mutex);
	wakeup_source_unregister(data->grip_ws);
	sysfs_remove_group(&data->input->dev.kobj, &sx938x_attribute_group);
#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
#else
	sensors_remove_symlink(data->input);
#endif
	input_unregister_device(data->input);
exit_input_init:
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[GRIP_%s] Probe fail!\n", grip_name[ic_num]);
	return ret;

}

static int sx938x_remove(struct i2c_client *client)
{
	struct sx938x_p *data = (struct sx938x_p *)i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == ON)
		sx938x_set_enable(data, OFF);

	sx938x_set_mode(data, SX938x_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	cancel_delayed_work_sync(&data->irq_work);
	cancel_delayed_work_sync(&data->debug_work);
	free_irq(data->irq, data);
	gpio_free(data->gpio_nirq);

	wakeup_source_unregister(data->grip_ws);
	sensors_unregister(data->factory_device, sensor_attrs);
#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
#else
	sensors_remove_symlink(data->input);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &sx938x_attribute_group);
	input_unregister_device(data->input);
	input_unregister_device(data->noti_input_dev);
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->read_mutex);

	kfree(data);

	return 0;
}

static int sx938x_suspend(struct device *dev)
{
	struct sx938x_p *data = dev_get_drvdata(dev);
	int cnt = 0;

	GRIP_INFO("\n");
	/* before go to sleep, make the interrupt pin as high*/
	while ((sx938x_get_nirq_state(data) == INTERRUPT_LOW) && (cnt++ < 3)) {
		sx938x_read_reg_stat(data);
		msleep(20);
	}
	if (cnt >= 3)
		GRIP_ERR("s/w reset fail(%d)\n", cnt);

	sx938x_set_debug_work(data, OFF, 1000);

	return 0;
}
static int sx938x_resume(struct device *dev)
{
	struct sx938x_p *data = dev_get_drvdata(dev);

	GRIP_INFO("\n");
	sx938x_set_debug_work(data, ON, 1000);

	return 0;
}

static void sx938x_shutdown(struct i2c_client *client)
{
	struct sx938x_p *data = i2c_get_clientdata(client);

	GRIP_INFO("\n");
	sx938x_set_debug_work(data, OFF, 1000);
	if (atomic_read(&data->enable) == ON)
		sx938x_set_enable(data, OFF);

	sx938x_set_mode(data, SX938x_MODE_SLEEP);
}


static const struct of_device_id sx938x_match_table[] = {
	{ .compatible = "sx938x",},
	{},
};

static const struct i2c_device_id sx938x_id[] = {
	{ "SX938X", 0 },
	{ }
};

static const struct dev_pm_ops sx938x_pm_ops = {
	.suspend = sx938x_suspend,
	.resume = sx938x_resume,
};

static struct i2c_driver sx938x_driver = {
	.driver = {
		.name	= "SX9380",
		.owner	= THIS_MODULE,
		.of_match_table = sx938x_match_table,
		.pm = &sx938x_pm_ops
	},
	.probe		= sx938x_probe,
	.remove		= sx938x_remove,
	.shutdown	= sx938x_shutdown,
	.id_table	= sx938x_id,
};

#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
static const struct of_device_id sx938x_sub_match_table[] = {
	{ .compatible = "sx938x_sub",},
	{},
};

static const struct i2c_device_id sx938x_sub_id[] = {
	{ "SX938X_SUB", 0 },
	{ }
};

static struct i2c_driver sx938x_sub_driver = {
	.driver = {
		.name	= "SX9380_SUB",
		.owner	= THIS_MODULE,
		.of_match_table = sx938x_sub_match_table,
		.pm = &sx938x_pm_ops
	},
	.probe		= sx938x_probe,
	.remove		= sx938x_remove,
	.shutdown	= sx938x_shutdown,
	.id_table	= sx938x_sub_id,
};
#endif

#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
static const struct of_device_id sx938x_sub2_match_table[] = {
	{ .compatible = "sx938x_sub2",},
	{},
};

static const struct i2c_device_id sx938x_sub2_id[] = {
	{ "SX938X_SUB2", 0 },
	{ }
};

static struct i2c_driver sx938x_sub2_driver = {
	.driver = {
		.name	= "SX9380_SUB2",
		.owner	= THIS_MODULE,
		.of_match_table = sx938x_sub2_match_table,
		.pm = &sx938x_pm_ops
	},
	.probe		= sx938x_probe,
	.remove		= sx938x_remove,
	.shutdown	= sx938x_shutdown,
	.id_table	= sx938x_sub2_id,
};
#endif

#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
static const struct of_device_id sx938x_wifi_match_table[] = {
	{ .compatible = "sx938x_wifi",},
	{},
};

static const struct i2c_device_id sx938x_wifi_id[] = {
	{ "SX938X_WIFI", 0 },
	{ }
};

static struct i2c_driver sx938x_wifi_driver = {
	.driver = {
		.name	= "SX9380_WIFI",
		.owner	= THIS_MODULE,
		.of_match_table = sx938x_wifi_match_table,
		.pm = &sx938x_pm_ops
	},
	.probe		= sx938x_probe,
	.remove		= sx938x_remove,
	.shutdown	= sx938x_shutdown,
	.id_table	= sx938x_wifi_id,
};
#endif

static int __init sx938x_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&sx938x_driver);
	if (ret != 0)
		pr_err("[GRIP] sx938x_driver probe fail\n");
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	ret = i2c_add_driver(&sx938x_sub_driver);
	if (ret != 0)
		pr_err("[GRIP_SUB] sx938x_sub_driver probe fail\n");
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	ret = i2c_add_driver(&sx938x_sub2_driver);
	if (ret != 0)
		pr_err("[GRIP_SUB] sx938x_sub2_driver probe fail\n");
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	ret = i2c_add_driver(&sx938x_wifi_driver);
	if (ret != 0)
		pr_err("[GRIP_WIFI] sx938x_wifi_driver probe fail\n");
#endif
	return ret;

}

static void __exit sx938x_exit(void)
{
	i2c_del_driver(&sx938x_driver);
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	i2c_del_driver(&sx938x_sub_driver);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
	i2c_del_driver(&sx938x_sub2_driver);
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
	i2c_del_driver(&sx938x_wifi_driver);
#endif

}

module_init(sx938x_init);
module_exit(sx938x_exit);


MODULE_DESCRIPTION("Semtech Corp. SX9380 Capacitive Touch Controller Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");


