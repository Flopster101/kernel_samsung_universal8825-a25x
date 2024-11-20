/*
 * SAMSUNG NFC Controller
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Woonki Lee <woonki84.lee@samsung.com>
 *         Heejae Kim <heejae12.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 */

/* duplicated #define pr_fmt(fmt)     "[sec_nfc] %s: " fmt, __func__*/

#include <linux/wait.h>
#include <linux/delay.h>

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/ktime.h>


#if defined(CONFIG_ESE_SECURE) && defined(CONFIG_ESE_USE_TZ_API)
extern int tz_tee_ese_secure_check(void);
enum secure_state {
	NOT_CHECKED,
	ESE_SECURED,
	ESE_NOT_SECURED,
};
int nfc_ese_secured;
#endif

#include "nfc_wakelock.h"
#include "sec_nfc.h"

#ifdef CONFIG_SEC_NFC_LOGGER
#include "./nfc_logger/nfc_logger.h"
#endif

#define SEC_NFC_GET_INFO(dev) i2c_get_clientdata(to_i2c_client(dev))

static int nfc_param_lpcharge = LPM_NO_SUPPORT;
module_param(nfc_param_lpcharge, int, 0440);

enum sec_nfc_irq {
	SEC_NFC_SKIP = -1,
	SEC_NFC_NONE,
	SEC_NFC_INT,
	SEC_NFC_READ_TIMES,
};

struct sec_nfc_i2c_info {
	struct i2c_client *i2c_dev;
	struct mutex read_mutex;
	enum sec_nfc_irq read_irq;
	wait_queue_head_t read_wait;
	size_t buflen;
	u8 *buf;
};

struct sec_nfc_info {
	struct miscdevice miscdev;
	struct mutex mutex;
	enum sec_nfc_mode mode;
	struct device *dev;
	struct sec_nfc_platform_data *pdata;
	struct sec_nfc_i2c_info i2c_info;
	struct nfc_wake_lock nfc_wake_lock;
	struct nfc_wake_lock nfc_clk_wake_lock;
	bool clk_ctl;
	bool clk_state;
	struct platform_device *pdev;
};

#ifdef CONFIG_ESE_COLDRESET
struct mutex coldreset_mutex;
struct mutex sleep_wake_mutex;
bool sleep_wakeup_state[2];
u8 disable_combo_reset_cmd[4] = { 0x2F, 0x30, 0x01, 0x00};
enum sec_nfc_mode cur_mode;
#endif

static struct sec_nfc_info *g_nfc_info;
#define FEATURE_SEC_NFC_TEST
#ifdef FEATURE_SEC_NFC_TEST
static bool on_nfc_test;
static bool nfc_int_wait;
#endif
static irqreturn_t sec_nfc_irq_thread_fn(int irq, void *dev_id)
{
	struct sec_nfc_info *info = dev_id;
	struct sec_nfc_platform_data *pdata = info->pdata;

	NFC_LOG_REC("irq\n");

#ifdef FEATURE_SEC_NFC_TEST
	if (on_nfc_test) {
		nfc_int_wait = true;
		NFC_LOG_INFO("NFC_TEST: interrupt is raised\n");
		wake_up_interruptible(&info->i2c_info.read_wait);
		return IRQ_HANDLED;
	}
#endif

	if (gpio_get_value(pdata->irq) == 0) {
		NFC_LOG_REC("irq-gpio state is low!\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&info->i2c_info.read_mutex);
	/* Skip interrupt during power switching
	 * It is released after first write
	 */
	if (info->i2c_info.read_irq == SEC_NFC_SKIP) {
		NFC_LOG_REC("Now power swiching. Skip this IRQ\n");
		mutex_unlock(&info->i2c_info.read_mutex);
		return IRQ_HANDLED;
	}

	info->i2c_info.read_irq += SEC_NFC_READ_TIMES;

#ifdef CONFIG_SEC_NFC_DUPLICATED_IRQ_WQ_LSI
	if (info->i2c_info.read_irq >= SEC_NFC_READ_TIMES * 2) {
		NFC_LOG_ERR("AP called duplicated IRQ handler\n");
		info->i2c_info.read_irq -= SEC_NFC_READ_TIMES;
		mutex_unlock(&info->i2c_info.read_mutex);
		return IRQ_HANDLED;
	}
#endif
	mutex_unlock(&info->i2c_info.read_mutex);

	wake_up_interruptible(&info->i2c_info.read_wait);
	wake_lock_timeout(&info->nfc_wake_lock, 2*HZ);

	return IRQ_HANDLED;
}

static int nfc_state_print(struct sec_nfc_info *info)
{
	struct sec_nfc_platform_data *pdata = info->pdata;
	struct regulator *regulator_nfc_pvdd;

	int en = gpio_get_value(info->pdata->ven);
	int firm = gpio_get_value_cansleep(info->pdata->firm);
	int irq = gpio_get_value(info->pdata->irq);
	int pvdd = 0;

	if (pdata->nfc_pvdd != NULL) {
		regulator_nfc_pvdd = pdata->nfc_pvdd;
		pvdd = regulator_is_enabled(regulator_nfc_pvdd);
	} else {
		pvdd = gpio_get_value(info->pdata->pvdd);
	}

	NFC_LOG_INFO("en(%d) firm(%d) pvdd(%d) irq(%d) mode(%d) clk_state(%d)\n",
			en, firm, pvdd, irq, info->mode, info->clk_state);

	return 0;
}

void sec_nfc_print_status(void)
{
	if (g_nfc_info)
		nfc_state_print(g_nfc_info);
}

static ssize_t sec_nfc_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	enum sec_nfc_irq irq;
	int ret = 0;

#ifdef FEATURE_SEC_NFC_TEST
	if (on_nfc_test)
		return 0;
#endif

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		NFC_LOG_ERR("read() nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	mutex_lock(&info->i2c_info.read_mutex);
	if (count == 0) {
		if (info->i2c_info.read_irq >= SEC_NFC_INT)
			info->i2c_info.read_irq--;
		mutex_unlock(&info->i2c_info.read_mutex);
		goto out;
	}

	irq = info->i2c_info.read_irq;
	mutex_unlock(&info->i2c_info.read_mutex);
	if (irq == SEC_NFC_NONE) {
		if (file->f_flags & O_NONBLOCK) {
			NFC_LOG_ERR("read() it is nonblock\n");
			ret = -EAGAIN;
			goto out;
		}
	}

	/* i2c recv */
	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		NFC_LOG_ERR("read() user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	NFC_LOG_REC("read(%zu)\n", count);
	mutex_lock(&info->i2c_info.read_mutex);
	memset(info->i2c_info.buf, 0, count);
	ret = i2c_master_recv(info->i2c_info.i2c_dev, info->i2c_info.buf, (u32)count);

	if (ret == -EREMOTEIO) {
		ret = -ERESTART;
		goto read_error;
	} else if (ret != count) {
		NFC_LOG_ERR("read failed: return: %d count: %d\n",
			ret, (u32)count);
		/*ret = -EREMOTEIO;*/
		goto read_error;
	}

	if (info->i2c_info.read_irq >= SEC_NFC_INT)
		info->i2c_info.read_irq--;

	if (info->i2c_info.read_irq == SEC_NFC_READ_TIMES)
		wake_up_interruptible(&info->i2c_info.read_wait);

	mutex_unlock(&info->i2c_info.read_mutex);

	if (copy_to_user(buf, info->i2c_info.buf, ret)) {
		NFC_LOG_ERR("read() copy failed to user\n");
		ret = -EFAULT;
	}

	goto out;

read_error:
	NFC_LOG_ERR("read error %d\n", ret);
	nfc_state_print(info);
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);
out:
	mutex_unlock(&info->mutex);

	return ret;
}

static ssize_t sec_nfc_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	int ret = 0;

	NFC_LOG_DBG("write() count %d\n", (u32)count);

#ifdef FEATURE_SEC_NFC_TEST
	if (on_nfc_test)
		return 0;
#endif

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		NFC_LOG_ERR("write() nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		NFC_LOG_ERR("write() user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(info->i2c_info.buf, buf, count)) {
		NFC_LOG_ERR("write() copy failed from user\n");
		ret = -EFAULT;
		goto out;
	}

	/* Skip interrupt during power switching
	 * It is released after first write
	 */
	NFC_LOG_REC("write(%d)\n", count);
	mutex_lock(&info->i2c_info.read_mutex);
	ret = i2c_master_send(info->i2c_info.i2c_dev, info->i2c_info.buf, count);
	if (info->i2c_info.read_irq == SEC_NFC_SKIP)
		info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);

	if (ret == -EREMOTEIO) {
		NFC_LOG_ERR("write failed: return: %d count: %d\n",
			ret, (u32)count);
		ret = -ERESTART;
		goto write_error;
	}

	if (ret != count) {
		NFC_LOG_ERR("write failed: return: %d count: %d\n",
			ret, (u32)count);
		ret = -EREMOTEIO;
		goto write_error;
	}

	goto out;

write_error:
	nfc_state_print(info);
out:
	mutex_unlock(&info->mutex);

	return ret;
}

static unsigned int sec_nfc_poll(struct file *file, poll_table *wait)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	enum sec_nfc_irq irq;

	int ret = 0;

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		NFC_LOG_ERR("poll() nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	poll_wait(file, &info->i2c_info.read_wait, wait);

	mutex_lock(&info->i2c_info.read_mutex);
	irq = info->i2c_info.read_irq;
	if (irq == SEC_NFC_READ_TIMES)
		ret = (POLLIN | POLLRDNORM);
	mutex_unlock(&info->i2c_info.read_mutex);

out:
	mutex_unlock(&info->mutex);

	return ret;
}

static int sec_nfc_regulator_onoff(struct sec_nfc_platform_data *data, int onoff)
{
	int rc = 0;
	struct regulator *regulator_nfc_pvdd = data->nfc_pvdd;

	if (!regulator_nfc_pvdd) {
		NFC_LOG_ERR("error: null regulator!\n");
		rc = -ENODEV;
		goto done;
	}

	NFC_LOG_INFO("regulator onoff = %d\n", onoff);

	if (onoff == NFC_I2C_LDO_ON) {
		rc = regulator_enable(regulator_nfc_pvdd);
		if (rc) {
			NFC_LOG_ERR("regulator enable nfc_pvdd failed, rc=%d\n", rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_nfc_pvdd);
		if (rc) {
			NFC_LOG_ERR("regulator disable nfc_pvdd failed, rc=%d\n", rc);
			goto done;
		}
	}

	NFC_LOG_INFO("success\n");
done:
	return rc;
}

void sec_nfc_i2c_irq_clear(struct sec_nfc_info *info)
{
	/* clear interrupt. Interrupt will be occurred at power off */
	mutex_lock(&info->i2c_info.read_mutex);
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);
}

int sec_nfc_i2c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sec_nfc_info *info = dev_get_drvdata(dev);
	struct sec_nfc_platform_data *pdata = info->pdata;
	int ret;

	NFC_LOG_INFO("probe() start\n");

	info->i2c_info.buflen = SEC_NFC_MAX_BUFFER_SIZE;
	info->i2c_info.buf = kzalloc(SEC_NFC_MAX_BUFFER_SIZE, GFP_KERNEL);
	if (!info->i2c_info.buf) {
		NFC_LOG_ERR("probe() failed to allocate memory\n");
		return -ENOMEM;
	}
	info->i2c_info.i2c_dev = client;
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_init(&info->i2c_info.read_mutex);
	init_waitqueue_head(&info->i2c_info.read_wait);
	i2c_set_clientdata(client, info);

	ret = gpio_request(pdata->irq, "nfc_int");
	if (ret) {
		NFC_LOG_ERR("probe() GPIO request is failed to register IRQ\n");
		goto err_irq_req;
	}
	gpio_direction_input(pdata->irq);

	ret = request_threaded_irq(client->irq, NULL, sec_nfc_irq_thread_fn,
#ifdef CONFIG_SEC_NFC_DUPLICATED_IRQ_WQ_QC
			IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND, SEC_NFC_DRIVER_NAME,
#else
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, SEC_NFC_DRIVER_NAME,
#endif
			info);
	if (ret < 0) {
		NFC_LOG_ERR("probe() failed to register IRQ handler\n");
		kfree(info->i2c_info.buf);
		return ret;
	}

	if (of_get_property(dev->of_node, "sec-nfc,ldo_control", NULL)) {
		if (pdata->nfc_pvdd != NULL) {
#if defined(CONFIG_NFC_PVDD_LATE_ENABLE)
			if (nfc_param_lpcharge == LPM_FALSE) {	/*pvdd enable in late init*/
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			if (!lpcharge) {
#else
			if (1/*!lpcharge*/) {
#endif
				ret = sec_nfc_regulator_onoff(pdata, NFC_I2C_LDO_ON);
				if (ret < 0)
					NFC_LOG_ERR("regulator on failed: %d\n", ret);

				if (of_find_property(dev->of_node, "sec-nfc,sw-gpio", NULL)) {
					if (gpio_is_valid(pdata->i2c_switch)) {
						ret = gpio_request(pdata->i2c_switch, "nfc_i2c_sw");
						if (ret)
							NFC_LOG_ERR("probe() i2c_swich gpio request failed\n");
						gpio_direction_output(pdata->i2c_switch, 1);
					}
				}
#ifdef CONFIG_ESE_COLDRESET
				msleep(25);
				gpio_set_value(pdata->ven, SEC_NFC_PW_ON);
#else
				usleep_range(1000, 1100);
#endif
			} else {
#if defined(CONFIG_NFC_PVDD_LATE_ENABLE)
				NFC_LOG_ERR("regulator off (late enable)\n");
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				NFC_LOG_ERR("regulator off at LPM: %d\n", lpcharge);
#else
				NFC_LOG_ERR("regulator off at LPM\n");
#endif
			}
		}
	} else {
		ret = gpio_request(pdata->pvdd, "nfc_pvdd");
		if (ret)
			NFC_LOG_ERR("probe() GPIO request is failed to register pvdd gpio\n");
		gpio_direction_output(pdata->pvdd, 1);
	}

	NFC_LOG_INFO("probe() success\n");
	return 0;

err_irq_req:
	return ret;
}

static irqreturn_t sec_nfc_clk_irq_thread(int irq, void *dev_id)
{
	struct sec_nfc_info *info = dev_id;
	struct sec_nfc_platform_data *pdata = info->pdata;
	bool value;

	if (pdata->irq_all_trigger) {
		value = gpio_get_value(pdata->clk_req) > 0 ? true : false;
		NFC_LOG_REC("clk_req: %d < %d\n", value, info->clk_state);

		if (value == info->clk_state)
			return IRQ_HANDLED;

		if (value) {
			if (!wake_lock_active(&info->nfc_clk_wake_lock))
				wake_lock(&info->nfc_clk_wake_lock);
			if (pdata->clk && clk_prepare_enable(pdata->clk)) {
				NFC_LOG_ERR("clock enable failed\n");
				return IRQ_HANDLED;
			}
		} else {
			if (pdata->clk)
				clk_disable_unprepare(pdata->clk);
			if (wake_lock_active(&info->nfc_clk_wake_lock))
				wake_unlock(&info->nfc_clk_wake_lock);
		}

		info->clk_state = value;
	} else {
		wake_lock_timeout(&info->nfc_wake_lock, 2*HZ);
		NFC_LOG_REC("clk_req irq\n");
	}

	return IRQ_HANDLED;
}

void sec_nfc_clk_ctl_enable(struct sec_nfc_info *info)
{
	struct sec_nfc_platform_data *pdata = info->pdata;

	if (info->clk_ctl)
		return;

	if (!pdata->clk)
		return;

	info->clk_state = false;
	info->clk_ctl = true;
}

void sec_nfc_clk_ctl_disable(struct sec_nfc_info *info)
{
	struct sec_nfc_platform_data *pdata = info->pdata;

	if (wake_lock_active(&info->nfc_clk_wake_lock))
		wake_unlock(&info->nfc_clk_wake_lock);

	if (!info->clk_ctl)
		return;

	if (!pdata->clk)
		return;

	if (info->clk_state)
		clk_disable_unprepare(pdata->clk);

	info->clk_state = false;
	info->clk_ctl = false;
}

static bool sec_nfc_check_pin_status(struct sec_nfc_platform_data *pdata,
					enum sec_nfc_mode mode)
{
	if (mode != SEC_NFC_MODE_OFF) {
		if (pdata->ven) {
			if (gpio_get_value(pdata->ven) != SEC_NFC_PW_ON)
				return false;
		}
	}

	if (mode == SEC_NFC_MODE_BOOTLOADER) {
		if (pdata->firm) {
			if (gpio_get_value_cansleep(pdata->firm) != SEC_NFC_FW_ON)
				return false;

		}
	} else {
		if (pdata->firm) {
			if (gpio_get_value_cansleep(pdata->firm) != SEC_NFC_FW_OFF)
				return false;
		}
	}

	return true;
}

static void sec_nfc_set_mode(struct sec_nfc_info *info,
					enum sec_nfc_mode mode)
{
	struct sec_nfc_platform_data *pdata = info->pdata;
	int retry_count = 3;
#ifdef CONFIG_ESE_COLDRESET
	int alreadFirmHigh = 0;
	int ret;
	enum sec_nfc_mode oldmode = info->mode;
#endif
	/* intfo lock is aleady gotten before calling this function */
	if (info->mode == mode) {
		NFC_LOG_DBG("power mode is already %d\n", mode);
		return;
	}
	info->mode = mode;

	/* Skip interrupt during power switching
	 * It is released after first write
	 */
	mutex_lock(&info->i2c_info.read_mutex);
	info->i2c_info.read_irq = SEC_NFC_SKIP;
	mutex_unlock(&info->i2c_info.read_mutex);
#ifdef CONFIG_ESE_COLDRESET
	mutex_lock(&sleep_wake_mutex);
	cur_mode = SEC_NFC_MODE_TURNING_ON_OFF;
	mutex_unlock(&sleep_wake_mutex);
	memset(sleep_wakeup_state, false, sizeof(sleep_wakeup_state));

	if (oldmode == SEC_NFC_MODE_OFF) {
		if (gpio_get_value_cansleep(pdata->firm) == 1) {
			alreadFirmHigh = 1;
			NFC_LOG_INFO("Firm is already high\n");
		} else {/*Firm pin is low*/
			gpio_set_value_cansleep(pdata->firm, SEC_NFC_FW_ON);
			msleep(SEC_NFC_VEN_WAIT_TIME);
		}

		if (gpio_get_value(pdata->ven) == SEC_NFC_PW_ON) {
			ret = i2c_master_send(info->i2c_info.i2c_dev, disable_combo_reset_cmd,
					sizeof(disable_combo_reset_cmd)/sizeof(u8));
			NFC_LOG_INFO("disable combo_reset_command ret: %d\n", ret);
		} else
			NFC_LOG_INFO("skip disable combo_reset_command\n");

		if (alreadFirmHigh == 1) {
			NFC_LOG_INFO("Firm is already HIGH\n");
		} else {/*Firm pin is low*/
			usleep_range(3000, 3100);
			gpio_set_value_cansleep(pdata->firm, SEC_NFC_FW_OFF);
		}
	}
#endif

#ifdef CONFIG_ESE_COLDRESET
	usleep_range(1000, 1100);
	NFC_LOG_INFO("FIRMWARE_GUARD_TIME(+1ms) in PW_OFF(total:4ms)\n");
#endif

pin_setting_retry:
	gpio_direction_output(pdata->ven, SEC_NFC_PW_OFF);
	if (pdata->firm)
		gpio_direction_output(pdata->firm, SEC_NFC_FW_OFF);

	if (mode == SEC_NFC_MODE_BOOTLOADER)
		if (pdata->firm)
			gpio_direction_output(pdata->firm, SEC_NFC_FW_ON);

	if (mode != SEC_NFC_MODE_OFF) {
		msleep(SEC_NFC_VEN_WAIT_TIME);
		gpio_direction_output(pdata->ven, SEC_NFC_PW_ON);
		sec_nfc_clk_ctl_enable(info);
		nfc_state_print(info);
		enable_irq_wake(info->i2c_info.i2c_dev->irq);
		msleep(SEC_NFC_VEN_WAIT_TIME/2);

		/* Workaround: FIRM or VEN is not set sometimes */
		if (retry_count-- > 0 && !sec_nfc_check_pin_status(pdata, mode)) {
			NFC_LOG_INFO("Pin setting retry\n");
			sec_nfc_clk_ctl_disable(info);
			disable_irq_wake(info->i2c_info.i2c_dev->irq);
			goto pin_setting_retry;
		}
#ifdef CONFIG_SEC_ESE_COLDRESET
		mutex_lock(&sleep_wake_mutex);
		cur_mode = mode;
		mutex_unlock(&sleep_wake_mutex);
#endif
	} else {
#ifdef CONFIG_ESE_COLDRESET
		int PW_OFF_DURATION = 20;
		ktime_t t0, t1;

		t0 = ktime_get();
		msleep(PW_OFF_DURATION);

		gpio_set_value(pdata->ven, SEC_NFC_PW_ON);
		t1 = ktime_get();

		NFC_LOG_INFO("DeepStby: PW_OFF duration (%d)ms, real PW_OFF duration is (%ld-%ld)ms\n",
									PW_OFF_DURATION, t0, t1);
		NFC_LOG_INFO("DeepStby: enter DeepStby(PW_ON)\n");
		mutex_lock(&sleep_wake_mutex);
		cur_mode = mode;
		mutex_unlock(&sleep_wake_mutex);
#endif
		sec_nfc_clk_ctl_disable(info);
		nfc_state_print(info);
		disable_irq_wake(info->i2c_info.i2c_dev->irq);
	}

	if (wake_lock_active(&info->nfc_wake_lock))
		wake_unlock(&info->nfc_wake_lock);

	NFC_LOG_INFO("NFC mode is : %d\n", mode);
}

#ifdef CONFIG_ESE_COLDRESET
struct cold_reset_gpio {
	int firm_gpio;
	int coldreset_gpio;
};

struct cold_reset_gpio cold_reset_gpio_data;

void init_coldreset_mutex(void)
{
	mutex_init(&coldreset_mutex);
}

void init_sleep_wake_mutex(void)
{
	mutex_init(&sleep_wake_mutex);
}

void check_and_sleep_nfc(unsigned int gpio, int value)
{
	if (sleep_wakeup_state[IDX_SLEEP_WAKEUP_NFC] == true ||
			sleep_wakeup_state[IDX_SLEEP_WAKEUP_ESE] == true) {
		NFC_LOG_INFO("%s keep wake up state\n", __func__);
		return;
	}
	gpio_set_value_cansleep(gpio, value);
}

int trig_cold_reset_id(int id)
{

	int wakeup_delay = 20;
	int duration = 18;
	ktime_t t0, t1, t2;
	int isFirmHigh = 0;

	NFC_LOG_INFO("COLDRESET: enter\n");

	if (id == ESE_ID)
		mutex_lock(&coldreset_mutex);

	NFC_LOG_INFO("caller id:(%d) coldreset triggered. [wakeup_delay(%d), duration(%d))]\n", id, wakeup_delay, duration);
	t0 = ktime_get();
	if (gpio_get_value_cansleep(cold_reset_gpio_data.firm_gpio) == 1) {
		isFirmHigh = 1;
	} else {
		gpio_set_value_cansleep(cold_reset_gpio_data.firm_gpio, SEC_NFC_FW_ON);
		msleep(wakeup_delay);
	}

	t1 = ktime_get();
	gpio_set_value(cold_reset_gpio_data.coldreset_gpio, SEC_NFC_COLDRESET_ON);
	usleep_range(duration * 1000, duration * 1000 + 10);
	gpio_set_value(cold_reset_gpio_data.coldreset_gpio, SEC_NFC_COLDRESET_OFF);
	t2 = ktime_get();

	if (isFirmHigh == 1)
		NFC_LOG_INFO("COLDRESET: FW_PIN already high, do not FW_OFF\n");
	else
		gpio_set_value_cansleep(cold_reset_gpio_data.firm_gpio, SEC_NFC_FW_OFF);

	NFC_LOG_INFO("COLDRESET: FW_ON time (%ld-%ld)\n", t0, t1);
	NFC_LOG_INFO("COLDRESET: GPIO3 ON time (%ld-%ld)\n", t1, t2);

	if (id == ESE_ID)
		mutex_unlock(&coldreset_mutex);

	NFC_LOG_INFO("COLDRESET: exit\n");
	return 0;
}

extern int trig_cold_reset(void)
{	/*only called GTO*/
	return trig_cold_reset_id(ESE_ID);
}

extern int trig_nfc_wakeup(void)
{
	NFC_LOG_INFO("%s\n", __func__);

	mutex_lock(&sleep_wake_mutex);
	if (cur_mode != SEC_NFC_MODE_FIRMWARE) {
		NFC_LOG_ERR("nfc mode not support to wake up\n");
		mutex_unlock(&sleep_wake_mutex);
		return -EPERM;
	}
	gpio_set_value_cansleep(cold_reset_gpio_data.firm_gpio, SEC_NFC_WAKE_UP);
	sleep_wakeup_state[IDX_SLEEP_WAKEUP_ESE] = true;
	mutex_unlock(&sleep_wake_mutex);
	return 0;
}

extern int trig_nfc_sleep(void)
{
	NFC_LOG_INFO("%s\n", __func__);

	mutex_lock(&sleep_wake_mutex);
	if (cur_mode != SEC_NFC_MODE_FIRMWARE) {
		NFC_LOG_ERR("nfc mode not support to sleep\n");
		mutex_unlock(&sleep_wake_mutex);
		return -EPERM;
	}
	sleep_wakeup_state[IDX_SLEEP_WAKEUP_ESE] = false;
	check_and_sleep_nfc(cold_reset_gpio_data.firm_gpio, SEC_NFC_WAKE_SLEEP);
	mutex_unlock(&sleep_wake_mutex);
	return 0;
}

#endif

static long sec_nfc_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	struct sec_nfc_platform_data *pdata = info->pdata;
	unsigned int new = (unsigned int)arg;
	int ret = 0;

	NFC_LOG_DBG("cmd: 0x%x\n", cmd);

	mutex_lock(&info->mutex);
#ifdef CONFIG_ESE_COLDRESET
	mutex_lock(&coldreset_mutex);
#endif

	switch (cmd) {
	case SEC_NFC_DEBUG:
		NFC_LOG_ERR("SEC_NFC_DEBUG\n");
		nfc_state_print(info);
		break;
	case SEC_NFC_SET_MODE:
		if (info->mode == new)
			break;

		if (new >= SEC_NFC_MODE_COUNT) {
			NFC_LOG_ERR("wrong mode (%d)\n", new);
			ret = -EFAULT;
			break;
		}
		sec_nfc_set_mode(info, new);

		break;

	case SEC_NFC_SLEEP:
		if (info->mode != SEC_NFC_MODE_BOOTLOADER) {
			if (wake_lock_active(&info->nfc_wake_lock))
				wake_unlock(&info->nfc_wake_lock);
#ifdef CONFIG_SEC_ESE_COLDRESET
			mutex_lock(&sleep_wake_mutex);
			sleep_wakeup_state[IDX_SLEEP_WAKEUP_NFC] = false;
			check_and_sleep_nfc(pdata->wake, SEC_NFC_WAKE_SLEEP);
			mutex_unlock(&sleep_wake_mutex);
#else
			gpio_set_value_cansleep(pdata->wake, SEC_NFC_WAKE_SLEEP);
#endif
		}
		break;

	case SEC_NFC_WAKEUP:
		if (info->mode != SEC_NFC_MODE_BOOTLOADER) {
			gpio_set_value_cansleep(pdata->wake, SEC_NFC_WAKE_UP);
#ifdef CONFIG_SEC_ESE_COLDRESET
			mutex_lock(&sleep_wake_mutex);
			sleep_wakeup_state[IDX_SLEEP_WAKEUP_NFC] = true;
			mutex_unlock(&sleep_wake_mutex);
#endif
			if (!wake_lock_active(&info->nfc_wake_lock))
				wake_lock(&info->nfc_wake_lock);
		}
		break;

/*[START] NPT*/
	case SEC_NFC_SET_NPT_MODE:
		NFC_LOG_INFO("NPT: VEN=%d, FIRM:%d\n", gpio_get_value(pdata->ven),
					gpio_get_value_cansleep(pdata->firm));

		if (new == SEC_NFC_NPT_CMD_ON) {
			NFC_LOG_INFO("NPT: NFC OFF mode NPT - Turn on VEN.\n");
			info->mode = SEC_NFC_MODE_FIRMWARE;
			mutex_lock(&info->i2c_info.read_mutex);
			info->i2c_info.read_irq = SEC_NFC_SKIP;
			mutex_unlock(&info->i2c_info.read_mutex);
			gpio_set_value(pdata->ven, SEC_NFC_PW_ON);
			sec_nfc_clk_ctl_enable(info);
			msleep(20);
			gpio_set_value_cansleep(pdata->firm, SEC_NFC_FW_ON);
			enable_irq_wake(info->i2c_info.i2c_dev->irq);
		} else if (new == SEC_NFC_NPT_CMD_OFF) {
			NFC_LOG_INFO("NPT: NFC OFF mode NPT - Turn off VEN.\n");
			info->mode = SEC_NFC_MODE_OFF;
			gpio_set_value_cansleep(pdata->firm, SEC_NFC_FW_OFF);
			gpio_set_value(pdata->ven, SEC_NFC_PW_OFF);
			sec_nfc_clk_ctl_disable(info);
			disable_irq_wake(info->i2c_info.i2c_dev->irq);
		}
		break;
/*[END] NPT*/

#ifdef CONFIG_ESE_COLDRESET
	case SEC_NFC_COLD_RESET:
		trig_cold_reset_id(DEVICEHOST_ID);
		break;
#endif
	default:
		NFC_LOG_ERR("NPT: Unknown ioctl 0x%x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

#ifdef CONFIG_ESE_COLDRESET
	mutex_unlock(&coldreset_mutex);
#endif
	mutex_unlock(&info->mutex);

	return ret;
}

static int sec_nfc_open(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	int ret = 0;

	NFC_LOG_INFO("%s\n", __func__);

#if defined(CONFIG_ESE_SECURE) && defined(CONFIG_ESE_USE_TZ_API)
	if (nfc_ese_secured == NOT_CHECKED) {
		ret = tz_tee_ese_secure_check();
		if (ret) {
			nfc_ese_secured = ESE_NOT_SECURED;
			NFC_LOG_ERR("eSE spi is not Secured\n");
			return -EBUSY;
		}
		nfc_ese_secured = ESE_SECURED;
	} else if (nfc_ese_secured == ESE_NOT_SECURED) {
		NFC_LOG_ERR("eSE spi is not Secured\n");
		return -EBUSY;
	}
#endif

	mutex_lock(&info->mutex);
	if (info->mode != SEC_NFC_MODE_OFF) {
		NFC_LOG_ERR("open() nfc is busy\n");
		nfc_state_print(info);
		ret = -EBUSY;
		goto out;
	}

	sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);

out:
	mutex_unlock(&info->mutex);
	return ret;
}

static int sec_nfc_close(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);

	if (wake_lock_active(&info->nfc_clk_wake_lock))
		wake_unlock(&info->nfc_clk_wake_lock);

	nfc_state_print(info);

	NFC_LOG_INFO("%s\n", __func__);

	mutex_lock(&info->mutex);
	sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);
	mutex_unlock(&info->mutex);

	return 0;
}

static const struct file_operations sec_nfc_fops = {
	.owner		= THIS_MODULE,
	.read		= sec_nfc_read,
	.write		= sec_nfc_write,
	.poll		= sec_nfc_poll,
	.open		= sec_nfc_open,
	.release	= sec_nfc_close,
	.unlocked_ioctl	= sec_nfc_ioctl,
};

#ifdef CONFIG_PM
static int sec_nfc_suspend(struct device *dev)
{
	struct sec_nfc_info *info = SEC_NFC_GET_INFO(dev);
	int ret = 0;

	NFC_LOG_INFO_WITH_DATE("suspend!\n");
	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_BOOTLOADER)
		ret = -EPERM;

	mutex_unlock(&info->mutex);

	return ret;
}

static int sec_nfc_resume(struct device *dev)
{
	NFC_LOG_INFO_WITH_DATE("resume!\n");

	return 0;
}

static SIMPLE_DEV_PM_OPS(sec_nfc_pm_ops, sec_nfc_suspend, sec_nfc_resume);
#endif

/*device tree parsing*/
static int sec_nfc_parse_dt(struct device *dev,
	struct sec_nfc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	static int retry_count = 3;

	pdata->ven = of_get_named_gpio(np, "sec-nfc,ven-gpio", 0);
	pdata->firm = of_get_named_gpio(np, "sec-nfc,firm-gpio", 0);
	pdata->wake = pdata->firm;
	pdata->irq = of_get_named_gpio(np, "sec-nfc,irq-gpio", 0);


#ifdef CONFIG_ESE_COLDRESET
	pdata->coldreset = of_get_named_gpio(np, "sec-nfc,coldreset-gpio", 0);
	NFC_LOG_INFO("parse_dt() coldreset : %d\n", pdata->coldreset);
	cold_reset_gpio_data.firm_gpio = pdata->firm;
	cold_reset_gpio_data.coldreset_gpio = pdata->coldreset;
#endif

	if (of_get_property(dev->of_node, "sec-nfc,ldo_control", NULL)) {
		pdata->nfc_pvdd = regulator_get(dev, "nfc_pvdd");
		if (IS_ERR(pdata->nfc_pvdd)) {
			NFC_LOG_ERR("get nfc_pvdd error\n");
			pdata->nfc_pvdd = NULL;
			if (--retry_count > 0)
				return -EPROBE_DEFER;
			else
				return -ENODEV;
		}
	} else {
		pdata->pvdd = of_get_named_gpio(np, "sec-nfc,pvdd-gpio", 0);
		NFC_LOG_INFO("parse_dt() pvdd : %d\n", pdata->pvdd);
	}

	if (of_find_property(dev->of_node, "sec-nfc,sw-gpio", NULL)) {
		pdata->i2c_switch = of_get_named_gpio(np, "sec-nfc,sw-gpio", 0);
		NFC_LOG_INFO("parse_dt() i2c switch : %d\n", pdata->i2c_switch);
	}

	pdata->clk_req = of_get_named_gpio(np, "sec-nfc,clk_req-gpio", 0);
	NFC_LOG_INFO("parse_dt() clk_req : %d\n", pdata->clk_req);

	pdata->clk_req_wake = of_property_read_bool(np, "sec-nfc,clk_req_wake");
	NFC_LOG_INFO("%s : sec-nfc,clk_req_wake: %s\n", __func__, pdata->clk_req_wake ? "true" : "false");

	if (of_find_property(np, "clocks", NULL)) {
		pdata->clk = clk_get(dev, "oscclk_nfc");
		if (IS_ERR(pdata->clk)) {
			NFC_LOG_ERR("probe() clk not found\n");
			pdata->clk = NULL;
		} else {
			NFC_LOG_INFO("parse_dt() found oscclk_nfc\n");
		}
	}

	if (of_property_read_bool(np, "sec-nfc,irq_all_trigger")) {
		pdata->irq_all_trigger = true;
		NFC_LOG_INFO("irq_all_trigger\n");
	}

	if (!of_property_read_u32(np, "sec-nfc,bootloader_ver", &pdata->bootloader_ver))
		NFC_LOG_INFO("bootloader_ver : %d\n", pdata->bootloader_ver);

	NFC_LOG_INFO("parse_dt() irq : %d, ven : %d, firm : %d\n",
			pdata->irq, pdata->ven, pdata->firm);

	return 0;
}

#ifdef FEATURE_SEC_NFC_TEST
static int sec_nfc_i2c_read(char *buf, int count)
{
	struct sec_nfc_info *info = g_nfc_info;
	int ret = 0;

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		NFC_LOG_ERR("NFC_TEST: sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	/* i2c recv */
	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		NFC_LOG_ERR("NFC_TEST: user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&info->i2c_info.read_mutex);
	memset(buf, 0, count);
	ret = i2c_master_recv(info->i2c_info.i2c_dev, buf, (u32)count);
	NFC_LOG_INFO("NFC_TEST: recv size : %d\n", ret);

	if (ret == -EREMOTEIO) {
		ret = -ERESTART;
		goto read_error;
	} else if (ret != count) {
		NFC_LOG_ERR("NFC_TEST: read failed: return: %d count: %d\n",
			ret, (u32)count);
		goto read_error;
	}

	mutex_unlock(&info->i2c_info.read_mutex);

	goto out;

read_error:
	info->i2c_info.read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->i2c_info.read_mutex);
out:
	mutex_unlock(&info->mutex);

	return ret;
}

static int sec_nfc_i2c_write(char *buf,	int count)
{
	struct sec_nfc_info *info = g_nfc_info;
	int ret = 0;

	mutex_lock(&info->mutex);

	if (info->mode == SEC_NFC_MODE_OFF) {
		NFC_LOG_ERR("NFC_TEST: sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	if (count > info->i2c_info.buflen)
		count = info->i2c_info.buflen;

	if (count > SEC_NFC_MSG_MAX_SIZE) {
		NFC_LOG_ERR("NFC_TEST: user required wrong size :%d\n", (u32)count);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&info->i2c_info.read_mutex);
	ret = i2c_master_send(info->i2c_info.i2c_dev, buf, count);
	mutex_unlock(&info->i2c_info.read_mutex);

	if (ret == -EREMOTEIO) {
		NFC_LOG_ERR("NFC_TEST: send failed: return: %d count: %d\n",
		ret, (u32)count);
		ret = -ERESTART;
		goto out;
	}

	if (ret != count) {
		NFC_LOG_ERR("NFC_TEST: send failed: return: %d count: %d\n",
		ret, (u32)count);
		ret = -EREMOTEIO;
	}

out:
	mutex_unlock(&info->mutex);

	return ret;
}

static ssize_t test_show(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	enum sec_nfc_mode old_mode = g_nfc_info->mode;
	int size;
	int ret = 0;
	int timeout = 1;

	on_nfc_test = true;
	nfc_int_wait = false;
	NFC_LOG_INFO("NFC_TEST: mode = %d, bootloader ver = %d\n", old_mode, g_nfc_info->pdata->bootloader_ver);

	sec_nfc_set_mode(g_nfc_info, SEC_NFC_MODE_BOOTLOADER);

	if (g_nfc_info->pdata->bootloader_ver > 4) { /* SEN4, SN4V, RN4V */
		char cmd[9] = {0x0, 0x1, 0x5, 0x0, 0x0, 0x14, 0x1, 0x0, 0x0};

		ret = sec_nfc_i2c_write(cmd, 9);
	} else {
		char cmd[4] = {0x0, 0x1, 0x0, 0x0};

		ret = sec_nfc_i2c_write(cmd, 4);
	}

	if (ret < 0) {
		NFC_LOG_INFO("NFC_TEST: i2c write error %d\n", ret);
		size = snprintf(buf, PAGE_SIZE, "NFC_TEST: i2c write error %d\n", ret);
		goto exit;
	}

	timeout = wait_event_interruptible_timeout(g_nfc_info->i2c_info.read_wait, nfc_int_wait, 100);
	ret = sec_nfc_i2c_read(buf, 16);
	if (ret < 0) {
		NFC_LOG_INFO("NFC_TEST: i2c read error %d\n", ret);
		size = snprintf(buf, PAGE_SIZE, "NFC_TEST: i2c read error %d\n", ret);
		goto exit;
	}

	NFC_LOG_INFO("NFC_TEST: BL ver: %02X %02X %02X %02X, INT: %s\n", buf[0],
				buf[1],	buf[2], buf[3], timeout ? "OK":"NOK");
	size = snprintf(buf, PAGE_SIZE, "BL ver: %02X.%02X.%02X.%02X, INT: %s\n", buf[0],
				buf[1], buf[2],	buf[3], timeout ? "OK":"NOK");

exit:
	sec_nfc_set_mode(g_nfc_info, old_mode);
	on_nfc_test = false;

	return size;
}

static CLASS_ATTR_RO(test);
#endif

#if defined(CONFIG_NFC_PVDD_LATE_ENABLE)
static ssize_t pvdd_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;

	if (!g_nfc_info) {
		NFC_LOG_ERR("%s nfc drv is NULL!", __func__);
		return size;
	}

	NFC_LOG_INFO("late_pvdd_en %c\n", buf[0]);

	if (buf[0] == '1') {
		ret = sec_nfc_regulator_onoff(g_nfc_info->pdata, NFC_I2C_LDO_ON);
		if (ret < 0)
			NFC_LOG_ERR("regulator on failed: %d\n", ret);

		if (of_find_property(g_nfc_info->dev->of_node, "sec-nfc,sw-gpio", NULL)) {
			if (gpio_is_valid(g_nfc_info->pdata->i2c_switch)) {
				ret = gpio_request(g_nfc_info->pdata->i2c_switch, "nfc_i2c_sw");
				if (ret)
					NFC_LOG_ERR("probe() i2c_swich gpio request failed\n");
				gpio_direction_output(g_nfc_info->pdata->i2c_switch, 1);
			}
		}
	}

	return size;
}
static CLASS_ATTR_WO(pvdd);
#endif

static ssize_t nfc_support_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	NFC_LOG_INFO("\n");
	return 0;
}
static CLASS_ATTR_RO(nfc_support);

static int __sec_nfc_probe(struct device *dev)
{
	struct sec_nfc_info *info;
	struct sec_nfc_platform_data *pdata = NULL;
	int ret = 0;

	struct class *nfc_class;

	NFC_LOG_INFO("probe start\n");
	if (dev->of_node) {
		pdata = devm_kzalloc(dev,
			sizeof(struct sec_nfc_platform_data), GFP_KERNEL);
		if (!pdata) {
			NFC_LOG_ERR("probe() Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = sec_nfc_parse_dt(dev, pdata);
		if (ret)
			return ret;
	} else {
		pdata = dev->platform_data;
	}

	if (!pdata) {
		NFC_LOG_ERR("probe() No platform data\n");
		ret = -ENOMEM;
		goto err_pdata;
	}

	info = kzalloc(sizeof(struct sec_nfc_info), GFP_KERNEL);
	if (!info) {
		NFC_LOG_ERR("probe() failed to allocate memory for sec_nfc_info\n");
		ret = -ENOMEM;
		goto err_info_alloc;
	}
	info->dev = dev;
	info->pdata = pdata;
	info->mode = SEC_NFC_MODE_OFF;

	mutex_init(&info->mutex);

	wake_lock_init(&info->nfc_wake_lock, WAKE_LOCK_SUSPEND, "nfc_wake_lock");
	wake_lock_init(&info->nfc_clk_wake_lock, WAKE_LOCK_SUSPEND, "nfc_clk_wake_lock");

	dev_set_drvdata(dev, info);

	/*separate NFC / non NFC using GPIO*/
	if (of_find_property(dev->of_node, "sec-nfc,check_nfc", NULL)) {
		int nfc_support = 0;

		nfc_support = gpio_get_value(of_get_named_gpio(dev->of_node, "sec-nfc,check_nfc", 0));
		if (nfc_support > 0) {
			NFC_LOG_INFO("nfc support: %d\n", nfc_support);
		} else {
			struct pinctrl *pinctrl = NULL;

			pinctrl = devm_pinctrl_get_select(dev, "nfc_nc");
			if (IS_ERR_OR_NULL(pinctrl))
				NFC_LOG_ERR("Failed to configure nfc NC pin\n");
			else
				devm_pinctrl_put(pinctrl);
			NFC_LOG_INFO("nfc not support: %d\n", nfc_support);
			return -ENXIO;
		}
	}

	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	info->miscdev.name = SEC_NFC_DRIVER_NAME;
	info->miscdev.fops = &sec_nfc_fops;
	info->miscdev.parent = dev;
	ret = misc_register(&info->miscdev);
	if (ret < 0) {
		NFC_LOG_ERR("probe() failed to register Device\n");
		goto err_dev_reg;
	}

	if (pdata->clk_req_wake || pdata->irq_all_trigger) {
		unsigned long irq_flag = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

		if (pdata->irq_all_trigger)
			irq_flag |= IRQF_TRIGGER_FALLING;

		ret = gpio_request(pdata->clk_req, "nfc_clk_req");
		if (ret)
			NFC_LOG_ERR("probe() failed to get clk_req\n");

		gpio_direction_input(pdata->clk_req);
		pdata->clk_irq = gpio_to_irq(pdata->clk_req);

		ret = request_threaded_irq(pdata->clk_irq, NULL, sec_nfc_clk_irq_thread,
				irq_flag, "sec-nfc_clk", info);
		if (ret < 0)
			NFC_LOG_ERR("probe() failed to register CLK REQ IRQ handler\n");
		else
			enable_irq_wake(pdata->clk_irq);
	}

	ret = gpio_request(pdata->ven, "nfc_ven");
	if (ret) {
		NFC_LOG_ERR("probe() failed to get gpio ven\n");
		goto err_gpio_ven;
	}
	gpio_direction_output(pdata->ven, SEC_NFC_PW_OFF);

	if (pdata->firm) {
		ret = gpio_request(pdata->firm, "nfc_firm");
		if (ret) {
			NFC_LOG_ERR("probe() failed to get gpio firm\n");
			goto err_gpio_firm;
		}
		gpio_direction_output(pdata->firm, SEC_NFC_FW_OFF);
	}
#ifdef CONFIG_ESE_COLDRESET
	init_coldreset_mutex();
	init_sleep_wake_mutex();
	memset(sleep_wakeup_state, false, sizeof(sleep_wakeup_state));
	ret = gpio_request(pdata->coldreset, "nfc_coldreset");
	if (ret) {
		dev_err(dev, "failed to get gpio coldreset(NFC-GPIO3)\n");
		goto err_gpio_coldreset;
	}
	gpio_direction_output(pdata->coldreset, SEC_NFC_COLDRESET_OFF);
#endif

	g_nfc_info = info;
#ifdef FEATURE_SEC_NFC_TEST
	nfc_class = class_create(THIS_MODULE, "nfc_test");
	if (IS_ERR(&nfc_class))
		NFC_LOG_ERR("NFC: failed to create nfc_test class\n");
	else {
		ret = class_create_file(nfc_class, &class_attr_test);
		if (ret)
			NFC_LOG_ERR("NFC: failed to create attr_test\n");
	}
#endif
	nfc_class = class_create(THIS_MODULE, "nfc_sec");
	if (IS_ERR(&nfc_class))
		NFC_LOG_ERR("NFC: failed to create nfc class\n");
	else {
		ret = class_create_file(nfc_class, &class_attr_nfc_support);
		if (ret)
			NFC_LOG_ERR("NFC: failed to create attr_nfc_support\n");
#if defined(CONFIG_NFC_PVDD_LATE_ENABLE)
		if (nfc_param_lpcharge == LPM_NO_SUPPORT) {
			ret = class_create_file(nfc_class, &class_attr_pvdd);
			if (ret)
				NFC_LOG_ERR("NFC: failed to create attr_pvdd\n");
		}
#endif
	}

	nfc_logger_register_nfc_stauts_func(sec_nfc_print_status);

	NFC_LOG_INFO("probe() success\n");

	return 0;

#ifdef CONFIG_ESE_COLDRESET
err_gpio_coldreset:
	gpio_free(pdata->coldreset);
#endif
err_gpio_firm:
	gpio_free(pdata->ven);
err_gpio_ven:
	free_irq(pdata->clk_irq, info);
	misc_deregister(&info->miscdev);
err_dev_reg:
	mutex_destroy(&info->mutex);
	kfree(info);
err_info_alloc:
	devm_kfree(dev, pdata);
err_pdata:
	return ret;
}

static int __sec_nfc_remove(struct device *dev)
{
	struct sec_nfc_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->i2c_info.i2c_dev;
	struct sec_nfc_platform_data *pdata = info->pdata;

	NFC_LOG_DBG("remove\n");

	misc_deregister(&info->miscdev);
	sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);
	free_irq(client->irq, info);
	free_irq(pdata->clk_irq, info);
	gpio_free(pdata->irq);
	gpio_set_value_cansleep(pdata->firm, 0);
	gpio_free(pdata->ven);
	if (pdata->firm)
		gpio_free(pdata->firm);

	wake_lock_destroy(&info->nfc_wake_lock);

	kfree(info);

	return 0;
}

#define SEC_NFC_INIT(driver)	i2c_add_driver(driver)
#define SEC_NFC_EXIT(driver)	i2c_del_driver(driver)

static int sec_nfc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;

	nfc_logger_init();

	ret = __sec_nfc_probe(&client->dev);
	if (ret)
		return ret;

	if (sec_nfc_i2c_probe(client))
		__sec_nfc_remove(&client->dev);

	return ret;
}

static int sec_nfc_remove(struct i2c_client *client)
{
	return __sec_nfc_remove(&client->dev);
}

static struct i2c_device_id sec_nfc_id_table[] = {
	{ SEC_NFC_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sec_nfc_id_table);

static const struct of_device_id nfc_match_table[] = {
	{ .compatible = SEC_NFC_DRIVER_NAME,},
	{},
};

static struct i2c_driver sec_nfc_driver = {
	.probe = sec_nfc_probe,
	.id_table = sec_nfc_id_table,
	.remove = sec_nfc_remove,
	.driver = {
		.name = SEC_NFC_DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &sec_nfc_pm_ops,
#endif
		.of_match_table = nfc_match_table,
		.suppress_bind_attrs = true,
	},
};

#if !IS_MODULE(CONFIG_SAMSUNG_NFC)
/*
 * if cmd line(nfc_sec.nfc_param_lpcharge) is not defined in bootloader,
 * this function is not called and LPM_NO_SUPPORT(-1) is assigned to nfc_param_lpcharge.
 */
static int __init nfc_lpcharge_func(char *str)
{
	pr_info("nfc_sec.nfc_param_lpcharge %s\n", str);
	if (str[0] == '1')
		nfc_param_lpcharge = LPM_TRUE;
	else
		nfc_param_lpcharge = LPM_FALSE;

	return 0;
}

early_param("nfc_sec.nfc_param_lpcharge", nfc_lpcharge_func);
#endif

#if IS_MODULE(CONFIG_SAMSUNG_NFC)
extern int spip3_dev_init(void);
extern void spip3_dev_exit(void);

static int __init sec_nfc_init(void)
{
#if IS_ENABLED(CONFIG_ESE_P3_LSI)
	spip3_dev_init();
#endif
	return SEC_NFC_INIT(&sec_nfc_driver);
}

static void __exit sec_nfc_exit(void)
{
#if IS_ENABLED(CONFIG_ESE_P3_LSI)
	spip3_dev_exit();
#endif
	SEC_NFC_EXIT(&sec_nfc_driver);
}
#else
static int __init sec_nfc_init(void)
{
	pr_info("%s lpcharge %d\n", __func__, nfc_param_lpcharge);

	if (nfc_param_lpcharge == LPM_TRUE)
		return 0;

	return SEC_NFC_INIT(&sec_nfc_driver);
}

static void __exit sec_nfc_exit(void)
{
	if (nfc_param_lpcharge != LPM_TRUE)
		SEC_NFC_EXIT(&sec_nfc_driver);
}
#endif

module_init(sec_nfc_init);
module_exit(sec_nfc_exit);

MODULE_DESCRIPTION("Samsung sec_nfc driver");
MODULE_LICENSE("GPL");
