/*
 * Copyright (C) 2020 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "fingerprint.h"
#include "et5xx.h"

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/pinctrl/consumer.h>
#include "../pinctrl/core.h"

static DECLARE_BITMAP(minors, N_SPI_MINORS);

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

struct debug_logger *g_logger;
static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);

static irqreturn_t et5xx_fingerprint_interrupt(int irq, void *dev_id)
{
	struct et5xx_data *etspi = (struct et5xx_data *)dev_id;

	etspi->int_count++;
	etspi->finger_on = 1;
	disable_irq_nosync(etspi->gpio_irq);
	wake_up_interruptible(&interrupt_waitq);
	__pm_wakeup_event(etspi->fp_signal_lock, 1 * HZ);
	pr_info("FPS triggered.int_count(%d) On(%d)\n",
		etspi->int_count, etspi->finger_on);
	etspi->interrupt_count++;
	return IRQ_HANDLED;
}

int et5xx_Interrupt_Init(
		struct et5xx_data *etspi,
		int int_ctrl,
		int detect_period,
		int detect_threshold)
{
	int retval = 0;

	etspi->finger_on = 0;
	etspi->int_count = 0;
	pr_info("int_ctrl = %d detect_period = %d detect_threshold = %d\n",
				int_ctrl,
				detect_period,
				detect_threshold);

	etspi->detect_period = detect_period;
	etspi->detect_threshold = detect_threshold;
	etspi->gpio_irq = gpio_to_irq(etspi->drdyPin);

	if (etspi->gpio_irq < 0) {
		pr_err("gpio_to_irq failed\n");
		retval = etspi->gpio_irq;
		goto done;
	}

	if (etspi->drdy_irq_flag == DRDY_IRQ_DISABLE) {
		if (request_irq(
			etspi->gpio_irq, et5xx_fingerprint_interrupt, int_ctrl, "et5xx_irq", etspi) < 0) {
			pr_err("drdy request_irq failed\n");
			retval = -EBUSY;
			goto done;
		} else {
			enable_irq_wake(etspi->gpio_irq);
			etspi->drdy_irq_flag = DRDY_IRQ_ENABLE;
		}
	}
done:
	return retval;
}

int et5xx_Interrupt_Free(struct et5xx_data *etspi)
{
	pr_info("Entry\n");

	if (etspi != NULL) {
		if (etspi->drdy_irq_flag == DRDY_IRQ_ENABLE) {
			if (!etspi->int_count)
				disable_irq_nosync(etspi->gpio_irq);

			disable_irq_wake(etspi->gpio_irq);
			free_irq(etspi->gpio_irq, etspi);
			etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
		}
		etspi->finger_on = 0;
		etspi->int_count = 0;
	}
	return 0;
}

void et5xx_Interrupt_Abort(struct et5xx_data *etspi)
{
	etspi->finger_on = 1;
	wake_up_interruptible(&interrupt_waitq);
}

unsigned int et5xx_fps_interrupt_poll(
		struct file *file,
		struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct et5xx_data *etspi = file->private_data;

	pr_debug("FPS fps_interrupt_poll, finger_on(%d), int_count(%d)\n",
		etspi->finger_on, etspi->int_count);

	if (!etspi->finger_on)
		poll_wait(file, &interrupt_waitq, wait);

	if (etspi->finger_on) {
		mask |= POLLIN | POLLRDNORM;
		etspi->finger_on = 0;
	}
	return mask;
}

/*-------------------------------------------------------------------------*/

static void et5xx_reset(struct et5xx_data *etspi)
{
	pr_info("Entry\n");

	gpio_set_value(etspi->sleepPin, 0);
	usleep_range(1050, 1100);
	gpio_set_value(etspi->sleepPin, 1);
	etspi->reset_count++;
}

static void et5xx_pin_control(struct et5xx_data *etspi,
	bool pin_set)
{
	int status = 0;

	etspi->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(etspi->pins_poweron)) {
			status = pinctrl_select_state(etspi->p, etspi->pins_poweron);
			if (status)
				pr_err("can't set pin default state\n");
			pr_info("idle\n");
		}
	} else {
		if (!IS_ERR(etspi->pins_poweroff)) {
			status = pinctrl_select_state(etspi->p, etspi->pins_poweroff);
			if (status)
				pr_err("can't set pin sleep state\n");
			pr_info("sleep\n");
		}
	}
}

static void et5xx_power_set(struct et5xx_data *etspi, int status)
{
	int rc = 0;

	if (etspi->ldo_pin) {
		pr_info("ldo\n");
		if (status == 1) {
			if (etspi->ldo_pin) {
				gpio_set_value(etspi->ldo_pin, 1);
				etspi->ldo_enabled = 1;
			}
		} else if (status == 0) {
			if (etspi->ldo_pin) {
				gpio_set_value(etspi->ldo_pin, 0);
				etspi->ldo_enabled = 0;
			}
		} else {
			pr_err("can't support this value. %d\n", status);
		}
	} else if (etspi->regulator_3p3 != NULL) {
		pr_info("regulator, status %d\n", status);
		if (status == 1) {
			if (regulator_is_enabled(etspi->regulator_3p3) == 0) {
				rc = regulator_enable(etspi->regulator_3p3);
				if (rc)
					pr_err("regulator enable failed, rc=%d\n", rc);
				else
					etspi->ldo_enabled = 1;
			} else
				pr_info("regulator is already enabled\n");
		} else if (status == 0) {
			if (regulator_is_enabled(etspi->regulator_3p3)) {
				rc = regulator_disable(etspi->regulator_3p3);
				if (rc)
					pr_err("regulator disable failed, rc=%d\n", rc);
				else
					etspi->ldo_enabled = 0;
			} else
				pr_info("regulator is already disabled\n");
		} else {
			pr_err("can't support this value. %d\n", status);
		}
	} else {
		pr_info("This HW revision does not support a power control\n");
	}
}

static void et5xx_power_control(struct et5xx_data *etspi, int status)
{
	pr_info("status = %d\n", status);
	if (status == 1) {
		et5xx_power_set(etspi, 1);
		et5xx_pin_control(etspi, 1);
		usleep_range(1600, 1650);
		if (etspi->sleepPin)
			gpio_set_value(etspi->sleepPin, 1);
		usleep_range(12000, 12050);
	} else if (status == 0) {
		if (etspi->sleepPin)
			gpio_set_value(etspi->sleepPin, 0);
		et5xx_power_set(etspi, 0);
		et5xx_pin_control(etspi, 0);
	} else {
		pr_err("can't support this value. %d\n", status);
	}
}

static ssize_t et5xx_read(struct file *filp,
						char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static ssize_t et5xx_write(struct file *filp,
						const char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static long et5xx_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct et5xx_data *etspi;
	u32 tmp;
	struct egis_ioc_transfer *ioc = NULL;
	struct egis_ioc_transfer_32 *ioc_32 = NULL;
	u64 tx_buffer_64, rx_buffer_64;
	u8 *buf, *address, *result, *fr;
	/* Check type and command number */
	if (_IOC_TYPE(cmd) != EGIS_IOC_MAGIC) {
		pr_err("_IOC_TYPE(cmd) != EGIS_IOC_MAGIC");
		return -ENOTTY;
	}

	if (!filp || !filp->private_data) {
		pr_err("NULL pointer passed\n");
		return -EINVAL;
	}

	if (IS_ERR((void __user *)arg)) {
		pr_err("invalid user space pointer %lu\n", arg);
		return -EINVAL;
	}

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	etspi = filp->private_data;
	mutex_lock(&etspi->buf_lock);

	/* segmented and/or full-duplex I/O request */
	if ((_IOC_NR(cmd)) != (_IOC_NR(EGIS_IOC_MESSAGE(0))) \
					|| (_IOC_DIR(cmd)) != _IOC_WRITE) {
		retval = -ENOTTY;
		goto out;
	}

	tmp = _IOC_SIZE(cmd);

	if (tmp == 0) {
		pr_err("ioc size error\n");
		retval = -EINVAL;
		goto out;
	} else if (tmp == sizeof(struct egis_ioc_transfer)) {
		/* platform 64bit / kernel 64bit */
		ioc = kmalloc(tmp, GFP_KERNEL);
		if (!ioc) {
			retval = -ENOMEM;
			goto out;
		}
		if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
			pr_err("%s __copy_from_user error\n", __func__);
			retval = -EFAULT;
			goto out;
		}
	} else if (tmp == sizeof(struct egis_ioc_transfer_32)) {
		/* platform 32bit / kernel 64bit */
		ioc_32 = kmalloc(tmp, GFP_KERNEL);
		if (ioc_32 == NULL) {
			retval = -ENOMEM;
			pr_err("%s ioc_32 kmalloc error\n", __func__);
			goto out;
		}
		if (__copy_from_user(ioc_32, (void __user *)arg, tmp)) {
			retval = -EFAULT;
			pr_err("%s ioc_32 copy_from_user error\n", __func__);
			goto out;
		}
		ioc = kmalloc(sizeof(struct egis_ioc_transfer), GFP_KERNEL);
		if (ioc == NULL) {
			retval = -ENOMEM;
			pr_err("%s ioc kmalloc error\n", __func__);
			goto out;
		}
		tx_buffer_64 = (u64)ioc_32->tx_buf;
		rx_buffer_64 = (u64)ioc_32->rx_buf;
		ioc->tx_buf = (u8 *)tx_buffer_64;
		ioc->rx_buf = (u8 *)rx_buffer_64;
		ioc->len = ioc_32->len;
		ioc->speed_hz = ioc_32->speed_hz;
		ioc->delay_usecs = ioc_32->delay_usecs;
		ioc->bits_per_word = ioc_32->bits_per_word;
		ioc->cs_change = ioc_32->cs_change;
		ioc->opcode = ioc_32->opcode;
		memcpy(ioc->pad, ioc_32->pad, 3);
		kfree(ioc_32);
	} else {
		pr_err("ioc size error %d\n", tmp);
		retval = -EINVAL;
		goto out;
	}

	switch (ioc->opcode) {
	/*
	 * Read register
	 * tx_buf include register address will be read
	 */
	case FP_REGISTER_READ:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_debug("etspi FP_REGISTER_READ\n");

		retval = et5xx_io_read_register(etspi, address, result);
		if (retval < 0)
			pr_err("FP_REGISTER_READ error retval = %d\n", retval);
		break;

	/*
	 * Write data to register
	 * tx_buf includes address and value will be wrote
	 */
	case FP_REGISTER_WRITE:
		buf = ioc->tx_buf;
		pr_debug("FP_REGISTER_WRITE\n");

		retval = et5xx_io_write_register(etspi, buf);
		if (retval < 0)
			pr_err("FP_REGISTER_WRITE error retval = %d\n", retval);
		break;

	case FP_REGISTER_BREAD:
		pr_debug("FP_REGISTER_BREAD\n");
		retval = et5xx_io_burst_read_register(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BREAD error retval = %d\n", retval);
		break;

	case FP_REGISTER_BWRITE:
		pr_debug("FP_REGISTER_BWRITE\n");
		retval = et5xx_io_burst_write_register(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BWRITE error retval = %d\n", retval);
		break;

	case FP_REGISTER_BREAD_BACKWARD:
		pr_debug("FP_REGISTER_BREAD_BACKWARD\n");
		retval = et5xx_io_burst_read_register_backward(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BREAD_BACKWARD error retval = %d\n", retval);
		break;

	case FP_REGISTER_BWRITE_BACKWARD:
		pr_debug("FP_REGISTER_BWRITE_BACKWARD\n");
		retval = et5xx_io_burst_write_register_backward(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BWRITE_BACKWARD error retval = %d\n", retval);
		break;

	case FP_NVM_READ:
		pr_debug("FP_NVM_READ, (%d)\n", etspi->clk_setting->spi_speed);
		retval = et5xx_io_nvm_read(etspi, ioc);
		if (retval < 0)
			pr_err("FP_NVM_READ error retval = %d\n", retval);
		retval = et5xx_io_nvm_off(etspi, ioc);
		if (retval < 0)
			pr_err("FP_NVM_OFF error retval = %d\n", retval);
		else
			pr_debug("FP_NVM_OFF\n");
		break;

	case FP_NVM_WRITE:
		pr_debug("FP_NVM_WRITE, (%d)\n", etspi->clk_setting->spi_speed);
		retval = et5xx_io_nvm_write(etspi, ioc);
		if (retval < 0)
			pr_err("FP_NVM_WRITE error retval = %d\n", retval);
		retval = et5xx_io_nvm_off(etspi, ioc);
		if (retval < 0)
			pr_err("FP_NVM_OFF error retval = %d\n", retval);
		else
			pr_debug("FP_NVM_OFF\n");
		break;

	case FP_NVM_OFF:
		pr_debug("FP_NVM_OFF\n");
		retval = et5xx_io_nvm_off(etspi, ioc);
		if (retval < 0)
			pr_err("FP_NVM_OFF error retval = %d\n", retval);
		break;

	case FP_VDM_READ:
		pr_debug("FP_VDM_READ\n");
		retval = et5xx_io_vdm_read(etspi, ioc);
		if (retval < 0)
			pr_err("FP_VDM_READ error retval = %d\n", retval);
		else
			pr_debug("FP_VDM_READ finished.\n");
		break;

	case FP_VDM_WRITE:
		pr_debug("FP_VDM_WRITE\n");
		retval = et5xx_io_vdm_write(etspi, ioc);
		if (retval < 0)
			pr_err("FP_VDM_WRITE error retval = %d\n", retval);
		else
			pr_debug("FP_VDM_WRTIE finished.\n");
		break;
	/*
	 * Get one frame data from sensor
	 */
	case FP_GET_ONE_IMG:
		fr = ioc->rx_buf;
		pr_debug("FP_GET_ONE_IMG\n");

		retval = et5xx_io_get_frame(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_GET_ONE_IMG error retval = %d\n", retval);
		break;

	case FP_SENSOR_RESET:
		pr_info("FP_SENSOR_RESET\n");
		et5xx_reset(etspi);
		break;

	case FP_RESET_SET:
		break;

	case FP_POWER_CONTROL:
	case FP_POWER_CONTROL_ET5XX:
		pr_info("FP_POWER_CONTROL, status = %d\n", ioc->len);
		et5xx_power_control(etspi, ioc->len);
		break;

	case FP_SET_SPI_CLOCK:
		pr_info("FP_SET_SPI_CLOCK, clock = %d\n", ioc->speed_hz);
		if (etspi->clk_setting->spi_speed != (unsigned int)ioc->speed_hz)
			spi_clk_disable(etspi->clk_setting);
		etspi->clk_setting->spi_speed = (unsigned int)ioc->speed_hz;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		etspi->spi->max_speed_hz = ioc->speed_hz;
#endif
		spi_clk_enable(etspi->clk_setting);
		break;

	/*
	 * Trigger initial routine
	 */
	case INT_TRIGGER_INIT:
		pr_debug("Trigger function init\n");
		retval = et5xx_Interrupt_Init(
				etspi,
				(int)ioc->pad[0],
				(int)ioc->pad[1],
				(int)ioc->pad[2]);
		break;
	/* trigger */
	case INT_TRIGGER_CLOSE:
		pr_debug("Trigger function close\n");
		retval = et5xx_Interrupt_Free(etspi);
		break;
	/* Poll Abort */
	case INT_TRIGGER_ABORT:
		pr_debug("Trigger function abort\n");
		et5xx_Interrupt_Abort(etspi);
		break;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	case FP_DISABLE_SPI_CLOCK:
		pr_info("FP_DISABLE_SPI_CLOCK\n");
		spi_clk_disable(etspi->clk_setting);
		break;
	case FP_CPU_SPEEDUP:
		pr_debug("FP_CPU_SPEEDUP\n");
		if (ioc->len)
			cpu_speedup_enable(etspi->boosting);
		else
			cpu_speedup_disable(etspi->boosting);
		break;
	case FP_SET_SENSOR_TYPE:
		set_sensor_type((int)ioc->len, &etspi->sensortype);
		break;
	case FP_SET_LOCKSCREEN:
		pr_info("FP_SET_LOCKSCREEN\n");
		break;
	case FP_SET_WAKE_UP_SIGNAL:
		pr_info("FP_SET_WAKE_UP_SIGNAL\n");
		break;
#endif
	case FP_SENSOR_ORIENT:
		pr_info("orient is %d", etspi->orient);

		retval = put_user(etspi->orient, (u8 __user *) (uintptr_t)ioc->rx_buf);
		if (retval != 0)
			pr_err("FP_SENSOR_ORIENT put_user fail: %d\n", retval);
		break;

	case FP_SPI_VALUE:
		etspi->spi_value = ioc->len;
		pr_info("spi_value: 0x%x\n", etspi->spi_value);
			break;
	case FP_IOCTL_RESERVED_01:
	case FP_IOCTL_RESERVED_02:
			break;
	default:
		pr_err("default error : %d\n", ioc->opcode);
		retval = -EFAULT;
		break;
	}

out:
	if (ioc != NULL)
		kfree(ioc);

	mutex_unlock(&etspi->buf_lock);
	if (retval < 0)
		pr_err("retval = %d\n", retval);
	return retval;
}

#ifdef CONFIG_COMPAT
static long et5xx_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return et5xx_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define et5xx_compat_ioctl NULL
#endif

static int et5xx_open(struct inode *inode, struct file *filp)
{
	struct et5xx_data *etspi;
	int	retval = -ENXIO;

	pr_info("Entry\n");
	mutex_lock(&device_list_lock);

	list_for_each_entry(etspi, &device_list, device_entry) {
		if (etspi->devt == inode->i_rdev) {
			retval = 0;
			break;
		}
	}
	if (retval == 0) {
		if (etspi->buf == NULL) {
			etspi->buf = kmalloc(bufsiz, GFP_KERNEL);
			if (etspi->buf == NULL) {
				pr_debug("open/ENOMEM\n");
				retval = -ENOMEM;
			}
		}
		if (retval == 0) {
			etspi->users++;
			filp->private_data = etspi;
			nonseekable_open(inode, filp);
			etspi->bufsiz = bufsiz;
		}
	} else
		pr_debug("nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);
	return retval;
}

static int et5xx_release(struct inode *inode, struct file *filp)
{
	struct et5xx_data *etspi;

	pr_info("Entry\n");
	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		int	dofree;
#endif

		kfree(etspi->buf);
		etspi->buf = NULL;

		/* ... after we unbound from the underlying device? */
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		spin_lock_irq(&etspi->spi_lock);
		dofree = (etspi->spi == NULL);
		spin_unlock_irq(&etspi->spi_lock);

		if (dofree)
			kfree(etspi);
#endif
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

int et5xx_platformInit(struct et5xx_data *etspi)
{
	int retval = 0;

	pr_info("Entry\n");
	/* gpio setting for ldo, ldo2, sleep, drdy pin */
	if (etspi != NULL) {
		etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;

		if (etspi->ldo_pin) {
			retval = gpio_request(etspi->ldo_pin, "et5xx_ldo_en");
			if (retval < 0) {
				pr_err("gpio_request et5xx_ldo_en failed\n");
				goto et5xx_platformInit_ldo_failed;
			}
			gpio_direction_output(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
			pr_info("ldo en value =%d\n", gpio_get_value(etspi->ldo_pin));
		}
		retval = gpio_request(etspi->sleepPin, "et5xx_sleep");
		if (retval < 0) {
			pr_err("gpio_requset et5xx_sleep failed\n");
			goto et5xx_platformInit_sleep_failed;
		}

		gpio_direction_output(etspi->sleepPin, 0);
		if (retval < 0) {
			pr_err("gpio_direction_output SLEEP failed\n");
			retval = -EBUSY;
			goto et5xx_platformInit_sleep_failed;
		}
		pr_info("sleep value =%d\n", gpio_get_value(etspi->sleepPin));

		retval = gpio_request(etspi->drdyPin, "et5xx_drdy");
		if (retval < 0) {
			pr_err("gpio_request et5xx_drdy failed\n");
			goto et5xx_platformInit_drdy_failed;
		}

		retval = gpio_direction_input(etspi->drdyPin);
		if (retval < 0) {
			pr_err("gpio_direction_input DRDY failed\n");
			goto et5xx_platformInit_gpio_init_failed;
		}
	} else {
		retval = -EFAULT;
		goto et5xx_platformInit_default_failed;
	}

#if KERNEL_VERSION(4, 19, 188) > LINUX_VERSION_CODE
	/* 4.19 R */
	wakeup_source_init(etspi->fp_signal_lock, "et5xx_sigwake_lock");
	/* 4.19 Q */
	if (!(etspi->fp_signal_lock)) {
		etspi->fp_signal_lock = wakeup_source_create("et5xx_sigwake_lock");
		if (etspi->fp_signal_lock)
			wakeup_source_add(etspi->fp_signal_lock);
	}
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	wakeup_source_init(etspi->clk_setting->spi_wake_lock, "et5xx_wake_lock");
	if (!(etspi->clk_setting->spi_wake_lock)) {
		etspi->clk_setting->spi_wake_lock = wakeup_source_create("et5xx_wake_lock");
		if (etspi->clk_setting->spi_wake_lock)
			wakeup_source_add(etspi->clk_setting->spi_wake_lock);
	}
#endif
#else
	/* 5.4 R */
	etspi->fp_signal_lock = wakeup_source_register(etspi->dev, "et5xx_sigwake_lock");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	etspi->clk_setting->spi_wake_lock = wakeup_source_register(etspi->dev, "et5xx_wake_lock");
#endif
#endif

	pr_info("successful status=%d\n", retval);
	return retval;
et5xx_platformInit_gpio_init_failed:
	gpio_free(etspi->drdyPin);
et5xx_platformInit_drdy_failed:
	gpio_free(etspi->sleepPin);
et5xx_platformInit_sleep_failed:
	gpio_free(etspi->ldo_pin);
et5xx_platformInit_ldo_failed:
et5xx_platformInit_default_failed:
	pr_err("is failed\n");
	return retval;
}

void et5xx_platformUninit(struct et5xx_data *etspi)
{
	pr_info("Entry\n");

	if (etspi != NULL) {
		disable_irq_wake(etspi->gpio_irq);
		disable_irq(etspi->gpio_irq);
		free_irq(etspi->gpio_irq, etspi);
		etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
		if (etspi->ldo_pin)
			gpio_free(etspi->ldo_pin);
		gpio_free(etspi->sleepPin);
		gpio_free(etspi->drdyPin);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		wakeup_source_unregister(etspi->clk_setting->spi_wake_lock);
#endif
		wakeup_source_unregister(etspi->fp_signal_lock);
	}
}

static int et5xx_parse_dt(struct device *dev, struct et5xx_data *etspi)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int retval = 0;
	int gpio;

	gpio = of_get_named_gpio_flags(np, "etspi-sleepPin", 0, &flags);
	if (gpio < 0) {
		retval = gpio;
		pr_err("fail to get sleepPin\n");
		goto dt_exit;
	} else {
		etspi->sleepPin = gpio;
		pr_info("sleepPin=%d\n", etspi->sleepPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-drdyPin",	0, &flags);
	if (gpio < 0) {
		retval = gpio;
		pr_err("fail to get drdyPin\n");
		goto dt_exit;
	} else {
		etspi->drdyPin = gpio;
		pr_info("drdyPin=%d\n", etspi->drdyPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-ldoPin",
		0, &flags);
	if (gpio < 0) {
		etspi->ldo_pin = 0;
		pr_err("fail to get ldo_pin\n");
	} else {
		etspi->ldo_pin = gpio;
		pr_info("ldo_pin=%d\n", etspi->ldo_pin);
	}

	if (of_property_read_string(np, "etspi-regulator", &etspi->btp_vdd) < 0) {
		pr_info("not use btp_regulator\n");
		etspi->btp_vdd = NULL;
	} else {
		etspi->regulator_3p3 = regulator_get(dev, etspi->btp_vdd);
		if (IS_ERR(etspi->regulator_3p3) ||
				(etspi->regulator_3p3) == NULL) {
			pr_info("fail to get regulator_3p3\n");
			etspi->regulator_3p3 = NULL;
			return -EINVAL;
		}
		pr_info("btp_regulator ok\n");
	}

	if (of_property_read_u32(np, "etspi-min_cpufreq_limit",
		&etspi->boosting->min_cpufreq_limit))
		etspi->boosting->min_cpufreq_limit = 0;

	if (of_property_read_string_index(np, "etspi-chipid", 0,
			(const char **)&etspi->chipid)) {
		etspi->chipid = "NULL";
	}
	pr_info("chipid: %s\n", etspi->chipid);

	if (of_property_read_u32(np, "etspi-orient", &etspi->orient))
		etspi->orient = 0;
	pr_info("orient: %d\n", etspi->orient);

	etspi->p = pinctrl_get_select_default(dev);
	if (IS_ERR(etspi->p)) {
		retval = -EINVAL;
		pr_err("failed pinctrl_get\n");
		goto dt_exit;
	}

#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	etspi->pins_poweroff = pinctrl_lookup_state(etspi->p, "pins_poweroff");
#else
	etspi->pins_poweroff = pinctrl_lookup_state(etspi->p, "pins_poweroff_tz");
#endif
	if (IS_ERR(etspi->pins_poweroff)) {
		pr_err(": could not get pins sleep_state (%li)\n",
			PTR_ERR(etspi->pins_poweroff));
		goto fail_pinctrl_get;
	}

#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	etspi->pins_poweron = pinctrl_lookup_state(etspi->p, "pins_poweron");
#else
	etspi->pins_poweron = pinctrl_lookup_state(etspi->p, "pins_poweron_tz");
#endif
	if (IS_ERR(etspi->pins_poweron)) {
		pr_err(": could not get pins idle_state (%li)\n",
			PTR_ERR(etspi->pins_poweron));
		goto fail_pinctrl_get;
	}

	pr_info("is successful\n");
	return retval;
fail_pinctrl_get:
		pinctrl_put(etspi->p);
dt_exit:
	pr_err("is failed\n");
	return retval;
}

static const struct file_operations et5xx_fops = {
	.owner = THIS_MODULE,
	.write = et5xx_write,
	.read = et5xx_read,
	.unlocked_ioctl = et5xx_ioctl,
	.compat_ioctl = et5xx_compat_ioctl,
	.open = et5xx_open,
	.release = et5xx_release,
	.llseek = no_llseek,
	.poll = et5xx_fps_interrupt_poll
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int et5xx_type_check(struct et5xx_data *etspi)
{
	u8 buf1, buf2, buf3, buf4, buf5, buf6, buf7;

	et5xx_power_control(etspi, 1);

	msleep(20);
	et5xx_read_register(etspi, 0x00, &buf1);
	if (buf1 != 0xAA) {
		etspi->sensortype = SENSOR_FAILED;
		pr_info("sensor not ready, status = %x\n", buf1);
		et5xx_power_control(etspi, 0);
		return -ENODEV;
	}

	et5xx_read_register(etspi, 0xFD, &buf1);
	et5xx_read_register(etspi, 0xFE, &buf2);
	et5xx_read_register(etspi, 0xFF, &buf3);

	et5xx_read_register(etspi, 0x20, &buf4);
	et5xx_read_register(etspi, 0x21, &buf5);
	et5xx_read_register(etspi, 0x23, &buf6);
	et5xx_read_register(etspi, 0x24, &buf7);

	et5xx_power_control(etspi, 0);

	pr_info("buf1-7: %x, %x, %x, %x, %x, %x, %x\n",
		buf1, buf2, buf3, buf4, buf5, buf6, buf7);

	/*
	 * type check return value
	 * ET510C : 0X00 / 0X66 / 0X00 / 0X33
	 * ET510D : 0x03 / 0x0A / 0x05
	 * ET516A : 0x00 / 0x10 / 0x05
	 * ET520  : 0x03 / 0x14 / 0x05
	 * ET520E  : 0x04 / 0x14 / 0x05
	 * ET523  : 0x00 / 0x17 / 0x05
	 */

	if ((buf1 == 0x00) && (buf2 == 0x10) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_OK;
		pr_info("sensor type is EGIS ET516A sensor\n");
	} else if ((buf1 == 0x03) && (buf2 == 0x0A) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_OK;
		pr_info("sensor type is EGIS ET510D sensor\n");
	} else if ((buf1 == 0x03) && (buf2 == 0x14) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_OK;
		pr_info("sensor type is EGIS ET520 sensor\n");
	} else if ((buf1 == 0x04) && (buf2 == 0x14) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_OK;
		pr_info("sensor type is EGIS ET520E sensor\n");
	} else if ((buf1 == 0x00) && (buf2 == 0x17) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_OK;
		pr_info("sensor type is EGIS ET523 sensor\n");
	} else if ((buf2 == 0x1C) && (buf3 == 0x05)) {
		etspi->sensortype = SENSOR_OK;
		pr_info("sensor type is EGIS ET528 sensor\n");
	} else {
		if ((buf4 == 0x00) && (buf5 == 0x66)
				&& (buf6 == 0x00) && (buf7 == 0x33)) {
			etspi->sensortype = SENSOR_OK;
			pr_info("sensor type is EGIS ET510C sensor\n");
		} else {
			etspi->sensortype = SENSOR_FAILED;
			pr_info("sensor type is FAILED\n");
			return -ENODEV;
		}
	}
	return 0;
}
#endif

static ssize_t bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			etspi->clk_setting->spi_speed);
}

static ssize_t type_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
	int status = 0;

	do {
		status = et5xx_type_check(etspi);
		pr_info("type (%u), retry (%d)\n", etspi->sensortype, retry);
	} while (!etspi->sensortype && ++retry < 3);

	if (status == -ENODEV)
		pr_info("type check fail\n");
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", etspi->sensortype);
}

static ssize_t vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", etspi->chipid);
}

static ssize_t adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", DETECT_ADM);
}

static ssize_t intcnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", etspi->interrupt_count);
}

static ssize_t intcnt_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "c")) {
		etspi->interrupt_count = 0;
		pr_info("initialization is done\n");
	}
	return size;
}

static ssize_t resetcnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", etspi->reset_count);
}

static ssize_t resetcnt_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "c")) {
		etspi->reset_count = 0;
		pr_info("initialization is done\n");
	}
	return size;
}

static DEVICE_ATTR_RO(bfs_values);
static DEVICE_ATTR_RO(type_check);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(adm);
static DEVICE_ATTR_RW(intcnt);
static DEVICE_ATTR_RW(resetcnt);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
	&dev_attr_intcnt,
	&dev_attr_resetcnt,
	NULL,
};

static void et5xx_work_func_debug(struct work_struct *work)
{
	struct debug_logger *logger;
	struct et5xx_data *etspi;

	logger = container_of(work, struct debug_logger, work_debug);
	etspi = dev_get_drvdata(logger->dev);
	pr_info("ldo: %d, sleep: %d, tz: %d, spi_value: 0x%x, type: %s\n",
		etspi->ldo_enabled, gpio_get_value(etspi->sleepPin),
		etspi->tz_mode, etspi->spi_value,
		etspi->sensortype > 0 ? etspi->chipid : sensor_status[etspi->sensortype + 2]);
}

static struct et5xx_data *alloc_platformdata(struct device *dev)
{
	struct et5xx_data *etspi;

	etspi = devm_kzalloc(dev, sizeof(struct et5xx_data), GFP_KERNEL);
	if (etspi == NULL)
		return NULL;

	etspi->clk_setting = devm_kzalloc(dev, sizeof(*etspi->clk_setting), GFP_KERNEL);
	if (etspi->clk_setting == NULL)
		return NULL;

	etspi->boosting = devm_kzalloc(dev, sizeof(*etspi->boosting), GFP_KERNEL);
	if (etspi->boosting == NULL)
		return NULL;

	etspi->logger = devm_kzalloc(dev, sizeof(*etspi->logger), GFP_KERNEL);
	if (etspi->logger == NULL)
		return NULL;

	return etspi;
}

/*-------------------------------------------------------------------------*/

static struct class *et5xx_class;

/*-------------------------------------------------------------------------*/

static int et5xx_probe_common(struct device *dev, struct et5xx_data *etspi)
{
	int retval;
	unsigned long minor;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
#endif

	pr_info("Entry\n");

	/* Initialize the driver data */
	etspi->dev = dev;
	etspi->logger->dev = dev;
	dev_set_drvdata(dev, etspi);
	spin_lock_init(&etspi->spi_lock);
	mutex_init(&etspi->buf_lock);
	mutex_init(&device_list_lock);
	INIT_LIST_HEAD(&etspi->device_entry);

	/* device tree call */
	if (dev->of_node) {
		retval = et5xx_parse_dt(dev, etspi);
		if (retval) {
			pr_err("Failed to parse DT\n");
			goto et5xx_probe_parse_dt_failed;
		}
	}

	/* platform init */
	retval = et5xx_platformInit(etspi);
	if (retval != 0) {
		pr_err("platforminit failed\n");
		goto et5xx_probe_platformInit_failed;
	}

	retval = spi_clk_register(etspi->clk_setting, dev);
	if (retval < 0) {
		pr_err("register spi clk failed\n");
		goto et5xx_probe_spi_clk_register_failed;
	}

	etspi->clk_setting->spi_speed = (unsigned int)SLOW_BAUD_RATE;
	etspi->reset_count = 0;
	etspi->interrupt_count = 0;
	etspi->spi_value = 0;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	do {
		retval = et5xx_type_check(etspi);
		pr_info("type (%u), retry (%d)\n", etspi->sensortype, retry);
	} while (!etspi->sensortype && ++retry < 3);
	if (retval == -ENODEV)
		pr_info("type check fail\n");
#endif

#if defined(DISABLED_GPIO_PROTECTION)
	et5xx_pin_control(etspi, 0);
#endif

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev_t;

		etspi->devt = MKDEV(ET5XX_MAJOR, (unsigned int)minor);
		dev_t = device_create(et5xx_class, dev,
				etspi->devt, etspi, "esfp0");
		retval = IS_ERR(dev_t) ? PTR_ERR(dev_t) : 0;
	} else {
		pr_err("no minor number available!\n");
		retval = -ENODEV;
		mutex_unlock(&device_list_lock);
		goto et5xx_create_failed;
	}
	if (retval == 0) {
		set_bit(minor, minors);
		list_add(&etspi->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	retval = fingerprint_register(etspi->fp_device,
		etspi, fp_attrs, "fingerprint");
	if (retval) {
		pr_err("sysfs register failed\n");
		goto et5xx_register_failed;
	}

	g_logger = etspi->logger;
	retval = set_fp_debug_timer(etspi->logger, et5xx_work_func_debug);
	if (retval)
		goto et5xx_sysfs_failed;
	enable_fp_debug_timer(etspi->logger);
	pr_info("is successful\n");

	return retval;

et5xx_sysfs_failed:
	fingerprint_unregister(etspi->fp_device, fp_attrs);
et5xx_register_failed:
	device_destroy(et5xx_class, etspi->devt);
	class_destroy(et5xx_class);
et5xx_create_failed:
	spi_clk_unregister(etspi->clk_setting);
et5xx_probe_spi_clk_register_failed:
	et5xx_platformUninit(etspi);
et5xx_probe_platformInit_failed:
et5xx_probe_parse_dt_failed:
	pr_err("is failed\n");

	return retval;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int et5xx_probe(struct platform_device *pdev)
{
	int retval = -ENOMEM;
	struct et5xx_data *etspi;

	pr_info("Platform_device Entry\n");
	etspi = alloc_platformdata(&pdev->dev);
	if (etspi == NULL)
		goto et5xx_platform_alloc_failed;

	etspi->sensortype = SENSOR_UNKNOWN;
	etspi->tz_mode = true;

	retval = et5xx_probe_common(&pdev->dev, etspi);
	if (retval)
		goto et5xx_platform_probe_failed;

	pr_info("is successful\n");
	return retval;

et5xx_platform_probe_failed:
	etspi = NULL;
et5xx_platform_alloc_failed:
	pr_err("is failed : %d\n", retval);
	return retval;
}
#else
static int et5xx_probe(struct spi_device *spi)
{
	int retval = -ENOMEM;
	struct et5xx_data *etspi;

	pr_info("spi_device Entry\n");
	etspi = alloc_platformdata(&spi->dev);
	if (etspi == NULL)
		goto et5xx_spi_alloc_failed;

	spi->bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi->chip_select = 0;
	etspi->spi = spi;
	etspi->tz_mode = false;

	retval = spi_setup(spi);
	if (retval != 0) {
		pr_err("spi_setup() is failed. status : %d\n", retval);
		goto et5xx_spi_set_setup_failed;
	}

	retval = et5xx_probe_common(&spi->dev, etspi);
	if (retval)
		goto et5xx_spi_probe_failed;

	pr_info("is successful\n");
	return retval;

et5xx_spi_probe_failed:
et5xx_spi_set_setup_failed:
	etspi = NULL;
et5xx_spi_alloc_failed:
	pr_err("is failed : %d\n", retval);
	return retval;
}
#endif

static int et5xx_remove_common(struct device *dev)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	if (etspi != NULL) {
		disable_fp_debug_timer(etspi->logger);
		et5xx_platformUninit(etspi);
		spi_clk_unregister(etspi->clk_setting);
		spin_lock_irq(&etspi->spi_lock);
		etspi->spi = NULL;
		spin_unlock_irq(&etspi->spi_lock);
		mutex_lock(&device_list_lock);
		fingerprint_unregister(etspi->fp_device, fp_attrs);
		list_del(&etspi->device_entry);
		device_destroy(et5xx_class, etspi->devt);
		clear_bit(MINOR(etspi->devt), minors);
		if (etspi->users == 0)
			etspi = NULL;
		mutex_unlock(&device_list_lock);
	}
	return 0;
}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int et5xx_remove(struct spi_device *spi) {
	et5xx_remove_common(&spi->dev);
#else
static int et5xx_remove(struct platform_device *pdev) {
	et5xx_remove_common(&pdev->dev);
#endif
	return 0;
}

static int et5xx_pm_suspend(struct device *dev)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	disable_fp_debug_timer(etspi->logger);
	return 0;
}

static int et5xx_pm_resume(struct device *dev)
{
	struct et5xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	enable_fp_debug_timer(etspi->logger);
	return 0;
}

static const struct dev_pm_ops et5xx_pm_ops = {
	.suspend = et5xx_pm_suspend,
	.resume = et5xx_pm_resume
};

static const struct of_device_id et5xx_match_table[] = {
	{ .compatible = "etspi,et5xx",},
	{},
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static struct spi_driver et5xx_spi_driver = {
#else
static struct platform_driver et5xx_spi_driver = {
#endif
	.driver = {
		.name =	"egis_fingerprint",
		.owner = THIS_MODULE,
		.pm = &et5xx_pm_ops,
		.of_match_table = et5xx_match_table
	},
	.probe = et5xx_probe,
	.remove = et5xx_remove,
};

/*-------------------------------------------------------------------------*/

static int __init et5xx_init(void)
{
	int retval;

	pr_info("Entry\n");

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	retval = register_chrdev(ET5XX_MAJOR, "egis_fingerprint", &et5xx_fops);
	if (retval < 0) {
		pr_err("register_chrdev error.status:%d\n",	retval);
		return retval;
	}

	et5xx_class = class_create(THIS_MODULE, "egis_fingerprint");
	if (IS_ERR(et5xx_class)) {
		pr_err("class_create error.\n");
		unregister_chrdev(ET5XX_MAJOR, et5xx_spi_driver.driver.name);
		return PTR_ERR(et5xx_class);
	}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	retval = spi_register_driver(&et5xx_spi_driver);
#else
	retval = platform_driver_register(&et5xx_spi_driver);
#endif
	if (retval < 0) {
		pr_err("register_driver error.\n");
		class_destroy(et5xx_class);
		unregister_chrdev(ET5XX_MAJOR, et5xx_spi_driver.driver.name);
		return retval;
	}

	pr_info("is successful\n");

	return retval;
}

static void __exit et5xx_exit(void)
{
	pr_info("Entry\n");

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	spi_unregister_driver(&et5xx_spi_driver);
#else
	platform_driver_unregister(&et5xx_spi_driver);
#endif
	class_destroy(et5xx_class);
	unregister_chrdev(ET5XX_MAJOR, et5xx_spi_driver.driver.name);
}

module_init(et5xx_init);
module_exit(et5xx_exit);

MODULE_AUTHOR("fp.sec@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Inc. ET5XX driver");
MODULE_LICENSE("GPL");
