/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
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
#include "el7xx.h"

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>

struct debug_logger *g_logger;

static void el7xx_reset(struct el7xx_data *etspi)
{
	pr_info("Entry\n");
	if (etspi->sleepPin) {
		gpio_set_value(etspi->sleepPin, 0);
		usleep_range(1050, 1100);
		gpio_set_value(etspi->sleepPin, 1);
		etspi->reset_count++;
	}
}

static void el7xx_reset_control(struct el7xx_data *etspi, int status)
{
	pr_debug("Entry\n");
	if (etspi->sleepPin)
		gpio_set_value(etspi->sleepPin, status);
}

void el7xx_pin_control(struct el7xx_data *etspi, bool pin_set)
{
	int retval = 0;

	if (IS_ERR(etspi->p))
		return;

	etspi->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(etspi->pins_poweron)) {
			retval = pinctrl_select_state(etspi->p, etspi->pins_poweron);
			if (retval)
				pr_err("can't set pin default state : %d\n", retval);
			pr_info("idle\n");
		}
	} else {
		if (!IS_ERR(etspi->pins_poweroff)) {
			retval = pinctrl_select_state(etspi->p, etspi->pins_poweroff);
			if (retval)
				pr_err("can't set pin sleep state : %d\n", retval);
			pr_info("sleep\n");
		}
	}
}

static void el7xx_power_control(struct el7xx_data *etspi, int status)
{
	int retval = 0;

	if (etspi->ldo_enabled == status) {
		pr_err("called duplicate\n");
		return;
	}
	pr_info("status = %d\n", status);
	if (status == 1) {
		if (etspi->regulator_3p3) {
			retval = regulator_enable(etspi->regulator_3p3);
			if (retval) {
				pr_err("regulator enable failed rc=%d\n", retval);
				goto regulator_failed;
			}
			etspi->ldo_enabled = 1;
			usleep_range(2300, 2350);
		} else if (etspi->ldo_pin) {
			gpio_set_value(etspi->ldo_pin, 1);
			etspi->ldo_enabled = 1;
			usleep_range(2100, 2150);
		}
		if (etspi->sleepPin) {
			gpio_set_value(etspi->sleepPin, 1);
			usleep_range(1100, 1150);
		}
		el7xx_pin_control(etspi, true);
		usleep_range(5000, 5050);
	} else if (status == 0) {
		el7xx_pin_control(etspi, false);
		if (etspi->sleepPin)
			gpio_set_value(etspi->sleepPin, 0);
		if (etspi->regulator_3p3) {
			retval = regulator_disable(etspi->regulator_3p3);
			if (retval) {
				pr_err("regulator disable failed rc=%d\n", retval);
				goto regulator_failed;
			}
			etspi->ldo_enabled = 0;
		} else if (etspi->ldo_pin) {
			gpio_set_value(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
		}
	} else {
		pr_err("can't support this value. %d\n", status);
	}

regulator_failed:
	return;
}

static ssize_t el7xx_read(struct file *filp,
						char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	return 0;
}

static ssize_t el7xx_write(struct file *filp,
						const char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	return 0;
}

static long el7xx_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct el7xx_data *etspi;
	u32 tmp;
	struct egis_ioc_transfer *ioc = NULL;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	u8 *buf, *address, *result, *fr;
#endif

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
	if (_IOC_NR(cmd) != _IOC_NR(EGIS_IOC_MESSAGE(0))
					|| _IOC_DIR(cmd) != _IOC_WRITE) {
		retval = -ENOTTY;
		goto el7xx_ioctl_out;
	}

	tmp = _IOC_SIZE(cmd);
	if ((tmp == 0) || (tmp % sizeof(struct egis_ioc_transfer)) != 0) {
		pr_err("ioc size error\n");
		retval = -EINVAL;
		goto el7xx_ioctl_out;
	}
	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc) {
		retval = -ENOMEM;
		goto el7xx_ioctl_out;
	}
	if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
		pr_err("__copy_from_user error\n");
		retval = -EFAULT;
		goto el7xx_ioctl_out;
	}

	switch (ioc->opcode) {
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	/*
	 * Read register
	 * tx_buf include register address will be read
	 */
	case FP_REGISTER_READ:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_debug("etspi FP_REGISTER_READ\n");
		retval = el7xx_io_read_register(etspi, address, result, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_READ error retval = %d\n", retval);
		break;
	case FP_REGISTER_BREAD:
		pr_debug("FP_REGISTER_BREAD\n");
		retval = el7xx_io_burst_read_register(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BREAD error retval = %d\n", retval);
		break;
	case FP_REGISTER_BREAD_BACKWARD:
		pr_debug("FP_REGISTER_BREAD_BACKWARD\n");
		retval = el7xx_io_burst_read_register_backward(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BREAD_BACKWARD error retval = %d\n", retval);
		break;
	/*
	 * Write data to register
	 * tx_buf includes address and value will be wrote
	 */
	case FP_REGISTER_WRITE:
		buf = ioc->tx_buf;
		pr_debug("FP_REGISTER_WRITE\n");
		retval = el7xx_io_write_register(etspi, buf, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_WRITE error retval = %d\n", retval);
		break;
	case FP_REGISTER_BWRITE:
		pr_debug("FP_REGISTER_BWRITE\n");
		retval = el7xx_io_burst_write_register(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BWRITE error retval = %d\n", retval);
		break;
	case FP_REGISTER_BWRITE_BACKWARD:
		pr_debug("FP_REGISTER_BWRITE_BACKWARD\n");
		retval = el7xx_io_burst_write_register_backward(etspi, ioc);
		if (retval < 0)
			pr_err("FP_REGISTER_BWRITE_BACKWARD error retval = %d\n", retval);
		break;

	case FP_EFUSE_READ:
		pr_debug("FP_EFUSE_READ\n");
		retval = el7xx_io_read_efuse(etspi, ioc);
		if (retval < 0)
			pr_err("FP_EFUSE_READ error retval = %d\n", retval);
		break;
	case FP_EFUSE_WRITE:
		pr_debug("FP_EFUSE_WRITE\n");
		retval = el7xx_io_write_efuse(etspi, ioc);
		if (retval < 0)
			pr_err("FP_EFUSE_WRITE error retval = %d\n", retval);
		break;
	case FP_GET_IMG:
		fr = ioc->rx_buf;
		pr_debug("FP_GET_IMG\n");
		retval = el7xx_io_get_frame(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_GET_IMG error retval = %d\n", retval);
		break;
	case FP_WRITE_IMG:
		fr = ioc->tx_buf;
		pr_debug("FP_WRITE_IMG\n");
		retval = el7xx_io_write_frame(etspi, fr, ioc->len);
		if (retval < 0)
			pr_err("FP_WRITE_IMG error retval = %d\n", retval);
		break;
	case FP_TRANSFER_COMMAND:
		pr_debug("FP_TRANSFER_COMMAND\n");
		retval = el7xx_io_transfer_command(etspi, ioc->tx_buf, ioc->rx_buf, ioc->len);
		if (retval < 0)
			pr_err("FP_TRANSFER_COMMAND error retval = %d\n", retval);
		break;
	case FP_SET_SPI_SETUP_CONF:
		pr_info("FP_SET_SPI_SETUP_CONF : %d\n", ioc->len);
		el7xx_spi_setup_conf(etspi, ioc->len);
		break;
#endif
	case FP_SENSOR_RESET:
		pr_info("FP_SENSOR_RESET\n");
		el7xx_reset(etspi);
		break;

	case FP_RESET_CONTROL:
		pr_info("FP_RESET_CONTROL, status = %d\n", ioc->len);
		el7xx_reset_control(etspi, ioc->len);
		break;

	case FP_POWER_CONTROL:
		pr_info("FP_POWER_CONTROL, status = %d\n", ioc->len);
		el7xx_power_control(etspi, ioc->len);
		break;
	case FP_SET_SPI_CLOCK:
		pr_info("FP_SET_SPI_CLOCK, clock = %d\n", ioc->speed_hz);
		etspi->clk_setting->spi_speed = (unsigned int)ioc->speed_hz;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		etspi->spi->max_speed_hz = ioc->speed_hz;
#endif
		spi_clk_enable(etspi->clk_setting);
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
#endif
	case FP_SPI_VALUE:
		etspi->spi_value = ioc->len;
		pr_info("spi_value: 0x%x\n", etspi->spi_value);
		break;

	case FP_MODEL_INFO:
		pr_info("modelinfo is %s\n", etspi->model_info);
		retval = copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf,
			etspi->model_info, 10);
		if (retval != 0)
			pr_err("FP_IOCTL_MODEL_INFO copy_to_user failed: %d\n", retval);
		break;

	case FP_IOCTL_RESERVED_01:
	case FP_IOCTL_RESERVED_02:
	case FP_IOCTL_RESERVED_03:
	case FP_IOCTL_RESERVED_04:
	case FP_IOCTL_RESERVED_05:
	case FP_IOCTL_RESERVED_06:
	case FP_IOCTL_RESERVED_07:
		break;

	default:
		retval = -EFAULT;
		break;

	}

el7xx_ioctl_out:
	if (ioc != NULL)
		kfree(ioc);

	mutex_unlock(&etspi->buf_lock);
	if (retval < 0)
		pr_err("retval = %d\n", retval);
	return retval;
}

#ifdef CONFIG_COMPAT
static long el7xx_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return el7xx_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define el7xx_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int el7xx_open(struct inode *inode, struct file *filp)
{
	struct el7xx_data *etspi;
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
		etspi->users++;
		filp->private_data = etspi;
		nonseekable_open(inode, filp);
	} else
		pr_debug("nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);
	return retval;
}

static int el7xx_release(struct inode *inode, struct file *filp)
{
	struct el7xx_data *etspi;

	pr_info("Entry\n");
	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		int dofree;
#endif

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

int el7xx_platformInit(struct el7xx_data *etspi)
{
	int retval = 0;

	pr_info("Entry\n");
	/* gpio setting for ldo, sleep pin */
	if (etspi != NULL) {
		if (etspi->btp_vdd) {
			etspi->regulator_3p3 = regulator_get(NULL, etspi->btp_vdd);
			if (IS_ERR(etspi->regulator_3p3)) {
				pr_err("regulator get failed\n");
				retval = PTR_ERR(etspi->regulator_3p3);
				goto el7xx_platformInit_ldo_failed;
			}
			regulator_set_load(etspi->regulator_3p3, 100000);
			pr_info("btp_regulator ok\n");
			etspi->ldo_enabled = 0;
		} else if (etspi->ldo_pin) {
			retval = gpio_request(etspi->ldo_pin, "el7xx_ldo_en");
			if (retval < 0) {
				pr_err("gpio_request el7xx_ldo_en failed\n");
				goto el7xx_platformInit_ldo_failed;
			}
			gpio_direction_output(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
		}

		retval = gpio_request(etspi->sleepPin, "el7xx_sleep");
		if (retval < 0) {
			pr_err("gpio_requset el7xx_sleep failed\n");
			goto el7xx_platformInit_sleep_failed;
		}
		gpio_direction_output(etspi->sleepPin, 0);
		if (retval < 0) {
			pr_err("gpio_direction_output SLEEP failed\n");
			retval = -EBUSY;
			goto el7xx_platformInit_sleep_failed;
		}

		if (etspi->sleepPin)
			pr_info("sleep value =%d\n", gpio_get_value(etspi->sleepPin));
		if (etspi->ldo_pin)
			pr_info("ldo en value =%d\n", gpio_get_value(etspi->ldo_pin));

#ifdef ENABLE_SENSORS_FPRINT_SECURE
#if KERNEL_VERSION(4, 19, 188) > LINUX_VERSION_CODE
		/* 4.19 R */
		wakeup_source_init(etspi->clk_setting->spi_wake_lock, "el7xx_wake_lock");
		/* 4.19 Q */
		if (!(etspi->clk_setting->spi_wake_lock)) {
			etspi->clk_setting->spi_wake_lock = wakeup_source_create("el7xx_wake_lock");
			if (etspi->clk_setting->spi_wake_lock)
				wakeup_source_add(etspi->clk_setting->spi_wake_lock);
		}
#else
		/* 5.4 */
		etspi->clk_setting->spi_wake_lock = wakeup_source_register(etspi->dev, "el7xx_wake_lock");
#endif
#endif
	} else {
		retval = -EFAULT;
	}

	pr_info("successful status=%d\n", retval);
	return retval;

el7xx_platformInit_sleep_failed:
	if (etspi->sleepPin)
		gpio_free(etspi->sleepPin);
	if (etspi->ldo_pin)
		gpio_free(etspi->ldo_pin);
el7xx_platformInit_ldo_failed:
	pr_err("is failed. %d\n", retval);
	return retval;
}

void el7xx_platformUninit(struct el7xx_data *etspi)
{
	pr_info("Entry\n");

	if (etspi != NULL) {
		el7xx_pin_control(etspi, false);
		if (etspi->regulator_3p3)
			regulator_put(etspi->regulator_3p3);
		else if (etspi->ldo_pin)
			gpio_free(etspi->ldo_pin);
		if (etspi->sleepPin)
			gpio_free(etspi->sleepPin);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		wakeup_source_unregister(etspi->clk_setting->spi_wake_lock);
#endif
	}
}

static int el7xx_parse_dt(struct device *dev, struct el7xx_data *etspi)
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

	gpio = of_get_named_gpio_flags(np, "etspi-ldoPin", 0, &flags);
	if (gpio < 0) {
		etspi->ldo_pin = 0;
		pr_info("not use ldo_pin\n");
	} else {
		etspi->ldo_pin = gpio;
		pr_info("ldo_pin=%d\n", etspi->ldo_pin);
	}

	if (of_property_read_string(np, "etspi-regulator", &etspi->btp_vdd) < 0) {
		pr_info("not use btp_regulator\n");
		etspi->btp_vdd = NULL;
		if (gpio < 0) {
			retval = gpio;
			pr_err("fail to get power\n");
			goto dt_exit;
		}
	}
	pr_info("regulator: %s\n", etspi->btp_vdd);

	if (of_property_read_u32(np, "etspi-min_cpufreq_limit",
		&etspi->boosting->min_cpufreq_limit))
		etspi->boosting->min_cpufreq_limit = 0;

	if (of_property_read_string_index(np, "etspi-chipid", 0,
			(const char **)&etspi->chipid)) {
		etspi->chipid = "NULL";
	}
	pr_info("chipid: %s\n", etspi->chipid);

	if (of_property_read_string_index(np, "etspi-modelinfo", 0,
			(const char **)&etspi->model_info)) {
		etspi->model_info = "NONE";
	}
	pr_info("modelinfo: %s\n", etspi->model_info);

	if (of_property_read_string_index(np, "etspi-position", 0,
			(const char **)&etspi->sensor_position)) {
		etspi->sensor_position = "0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00";
	}
	pr_info("position: %s\n", etspi->sensor_position);

	if (of_property_read_string_index(np, "etspi-rb", 0,
			(const char **)&etspi->rb)) {
		etspi->rb = "525,-1,-1";
	}
	pr_info("rb: %s\n", etspi->rb);

	etspi->p = pinctrl_get_select_default(dev);
	if (IS_ERR(etspi->p)) {
		retval = PTR_ERR(etspi->p);
		pr_err("failed pinctrl_get\n");
		goto dt_exit;
	}
#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	etspi->pins_poweroff = pinctrl_lookup_state(etspi->p, "pins_poweroff");
	if (IS_ERR(etspi->pins_poweroff)) {
		pr_err("could not get pins sleep_state (%li)\n",
			PTR_ERR(etspi->pins_poweroff));
	}

	etspi->pins_poweron = pinctrl_lookup_state(etspi->p, "pins_poweron");
	if (IS_ERR(etspi->pins_poweron)) {
		pr_err("could not get pins idle_state (%li)\n",
			PTR_ERR(etspi->pins_poweron));
	}
#endif
	el7xx_pin_control(etspi, false);
	pr_info("is successful\n");
	return retval;

dt_exit:
	pr_err("is failed. %d\n", retval);
	return retval;
}

static const struct file_operations el7xx_fops = {
	.owner = THIS_MODULE,
	.write = el7xx_write,
	.read = el7xx_read,
	.unlocked_ioctl = el7xx_ioctl,
	.compat_ioctl = el7xx_compat_ioctl,
	.open = el7xx_open,
	.release = el7xx_release,
	.llseek = no_llseek,
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int el7xx_type_check(struct el7xx_data *etspi)
{
	u8 buf1, buf2, buf3;

	el7xx_power_control(etspi, 1);

	msleep(20);

	el7xx_read_register(etspi, 0x00, &buf1);
	el7xx_read_register(etspi, 0x01, &buf2);
	el7xx_read_register(etspi, 0x02, &buf3);

	el7xx_power_control(etspi, 0);

	pr_info("buf1-3: %x, %x, %x\n", buf1, buf2, buf3);

	/*
	 * type check return value
	 * EL721  : 0x07 / 0x15
	 */
	if ((buf1 == 0x07) && (buf2 == 0x15)) {
		etspi->sensortype = SENSOR_OK;
		pr_info("sensor type is EGIS EL721 sensor\n");
	} else {
		etspi->sensortype = SENSOR_FAILED;
		pr_info("sensor type is FAILED\n");
		return -ENODEV;
	}
	return 0;
}
#endif

static ssize_t bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct el7xx_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			data->clk_setting->spi_speed);
}

static ssize_t type_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct el7xx_data *data = dev_get_drvdata(dev);
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
	int retval = 0;

	do {
		retval = el7xx_type_check(data);
		pr_info("type (%u), retry (%d)\n",
			data->sensortype, retry);
	} while (!data->sensortype && ++retry < 3);

	if (retval == -ENODEV)
		pr_info("type check fail\n");
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", data->sensortype);
}

static ssize_t vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", etspi->chipid);
}

static ssize_t adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", DETECT_ADM);
}

static ssize_t position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", etspi->sensor_position);
}

static ssize_t rb_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", etspi->rb);
}

static ssize_t resetcnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", etspi->reset_count);
}

static ssize_t resetcnt_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

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
static DEVICE_ATTR_RW(resetcnt);
static DEVICE_ATTR_RO(position);
static DEVICE_ATTR_RO(rb);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
	&dev_attr_resetcnt,
	&dev_attr_position,
	&dev_attr_rb,
	NULL,
};

static void el7xx_work_func_debug(struct work_struct *work)
{
	struct debug_logger *logger;
	struct el7xx_data *etspi;

	logger = container_of(work, struct debug_logger, work_debug);
	etspi = dev_get_drvdata(logger->dev);
	pr_info("ldo: %d, sleep: %d, tz: %d, spi_value: 0x%x, type: %s\n",
		etspi->ldo_enabled, gpio_get_value(etspi->sleepPin),
		etspi->tz_mode, etspi->spi_value,
		etspi->sensortype > 0 ? etspi->chipid : sensor_status[etspi->sensortype + 2]);
}

static struct el7xx_data *alloc_platformdata(struct device *dev)
{
	struct el7xx_data *etspi;

	etspi = devm_kzalloc(dev, sizeof(struct el7xx_data), GFP_KERNEL);
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

static struct class *el7xx_class;

static int el7xx_probe_common(struct device *dev, struct el7xx_data *etspi)
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
		retval = el7xx_parse_dt(dev, etspi);
		if (retval) {
			pr_err("Failed to parse DT\n");
			goto el7xx_probe_parse_dt_failed;
		}
	}
	/* platform init */
	retval = el7xx_platformInit(etspi);
	if (retval != 0) {
		pr_err("platforminit failed\n");
		goto el7xx_probe_platformInit_failed;
	}

	retval = spi_clk_register(etspi->clk_setting, dev);
	if (retval < 0) {
		pr_err("register spi clk failed\n");
		goto el7xx_probe_spi_clk_register_failed;
	}

	etspi->reset_count = 0;
	etspi->clk_setting->enabled_clk = false;
	etspi->spi_value = 0;
	etspi->clk_setting->spi_speed = (unsigned int)SLOW_BAUD_RATE;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	do {
		retval = el7xx_type_check(etspi);
		pr_info("type (%u), retry (%d)\n", etspi->sensortype, retry);
	} while (!etspi->sensortype && ++retry < 3);

	if (retval == -ENODEV)
		pr_info("type check fail\n");
#endif

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev_t;

		etspi->devt = MKDEV(EL7XX_MAJOR, minor);
		dev_t = device_create(el7xx_class, dev,
				etspi->devt, etspi, "esfp0");
		retval = IS_ERR(dev_t) ? PTR_ERR(dev_t) : 0;
	} else {
		pr_err("no minor number available!\n");
		retval = -ENODEV;
		mutex_unlock(&device_list_lock);
		goto el7xx_device_create_failed;
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
		goto el7xx_register_failed;
	}

	g_logger = etspi->logger;
	retval = set_fp_debug_timer(etspi->logger, el7xx_work_func_debug);
	if (retval)
		goto el7xx_sysfs_failed;
	enable_fp_debug_timer(etspi->logger);
	pr_info("is successful\n");
	return retval;

el7xx_sysfs_failed:
	fingerprint_unregister(etspi->fp_device, fp_attrs);
el7xx_register_failed:
	device_destroy(el7xx_class, etspi->devt);
	class_destroy(el7xx_class);
el7xx_device_create_failed:
	spi_clk_unregister(etspi->clk_setting);
el7xx_probe_spi_clk_register_failed:
	el7xx_platformUninit(etspi);
el7xx_probe_platformInit_failed:
el7xx_probe_parse_dt_failed:
	pr_err("is failed : %d\n", retval);

	return retval;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int el7xx_probe(struct platform_device *pdev)
{
	int retval = -ENOMEM;
	struct el7xx_data *etspi;

	pr_info("Platform_device Entry\n");
	etspi = alloc_platformdata(&pdev->dev);
	if (etspi == NULL)
		goto el7xx_platform_alloc_failed;

	etspi->sensortype = SENSOR_UNKNOWN;
	etspi->tz_mode = true;

	retval = el7xx_probe_common(&pdev->dev, etspi);
	if (retval)
		goto el7xx_platform_probe_failed;

	pr_info("is successful\n");
	return retval;

el7xx_platform_probe_failed:
	etspi = NULL;
el7xx_platform_alloc_failed:
	pr_err("is failed : %d\n", retval);
	return retval;
}
#else
static int el7xx_probe(struct spi_device *spi)
{
	int retval = -ENOMEM;
	struct el7xx_data *etspi;

	pr_info("spi_device Entry\n");
	etspi = alloc_platformdata(&spi->dev);
	if (etspi == NULL)
		goto el7xx_spi_alloc_failed;

	spi->bits_per_word = 8;
	etspi->prev_bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi->chip_select = 0;
	etspi->spi = spi;
	etspi->tz_mode = false;

	spi_get_ctrldata(spi);

	retval = spi_setup(spi);
	if (retval != 0) {
		pr_err("spi_setup() is failed. status : %d\n", retval);
		goto el7xx_spi_set_setup_failed;
	}

	/* init transfer buffer */
	retval = el7xx_init_buffer(etspi);
	if (retval < 0) {
		pr_err("Failed to Init transfer buffer.\n");
		goto el7xx_spi_init_buffer_failed;
	}

	retval = el7xx_probe_common(&spi->dev, etspi);
	if (retval)
		goto el7xx_spi_probe_failed;

	pr_info("is successful\n");
	return retval;

el7xx_spi_probe_failed:
	el7xx_free_buffer(etspi);
el7xx_spi_init_buffer_failed:
el7xx_spi_set_setup_failed:
	etspi = NULL;
el7xx_spi_alloc_failed:
	pr_err("is failed : %d\n", retval);
	return retval;
}
#endif

static int el7xx_remove_common(struct device *dev)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	if (etspi != NULL) {
		disable_fp_debug_timer(etspi->logger);
		el7xx_platformUninit(etspi);
		spi_clk_unregister(etspi->clk_setting);
		/* make sure ops on existing fds can abort cleanly */
		spin_lock_irq(&etspi->spi_lock);
		etspi->spi = NULL;
		spin_unlock_irq(&etspi->spi_lock);

		/* prevent new opens */
		mutex_lock(&device_list_lock);
		fingerprint_unregister(etspi->fp_device, fp_attrs);
		list_del(&etspi->device_entry);
		device_destroy(el7xx_class, etspi->devt);
		clear_bit(MINOR(etspi->devt), minors);
		if (etspi->users == 0)
			etspi = NULL;
		mutex_unlock(&device_list_lock);
	}
	return 0;
}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
#if LINUX_VERSION_CODE > KERNEL_VERSION(6, 1, 0)
static void el7xx_remove(struct spi_device *spi)
{
	struct el7xx_data *etspi = spi_get_drvdata(spi);

	el7xx_free_buffer(etspi);
	el7xx_remove_common(&spi->dev);
	return;
}
#else
static int el7xx_remove(struct spi_device *spi)
{
	struct el7xx_data *etspi = spi_get_drvdata(spi);

	el7xx_free_buffer(etspi);
	el7xx_remove_common(&spi->dev);
	return 0;
}
#endif /* LINUX_VERSION_CODE */
#else
static int el7xx_remove(struct platform_device *pdev)
{
	el7xx_remove_common(&pdev->dev);
	return 0;
}
#endif /* ENABLE_SENSORS_FPRINT_SECURE */

static int el7xx_pm_suspend(struct device *dev)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	disable_fp_debug_timer(etspi->logger);
	return 0;
}

static int el7xx_pm_resume(struct device *dev)
{
	struct el7xx_data *etspi = dev_get_drvdata(dev);

	pr_info("Entry\n");
	enable_fp_debug_timer(etspi->logger);
	return 0;
}

static const struct dev_pm_ops el7xx_pm_ops = {
	.suspend = el7xx_pm_suspend,
	.resume = el7xx_pm_resume
};

static const struct of_device_id el7xx_match_table[] = {
	{.compatible = "etspi,el7xx",},
	{},
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static struct spi_driver el7xx_spi_driver = {
#else
static struct platform_driver el7xx_spi_driver = {
#endif
	.driver = {
		.name =	"egis_fingerprint",
		.owner = THIS_MODULE,
		.pm = &el7xx_pm_ops,
		.of_match_table = el7xx_match_table
	},
	.probe = el7xx_probe,
	.remove = el7xx_remove,
};

/*-------------------------------------------------------------------------*/

static int __init el7xx_init(void)
{
	int retval = 0;

	pr_info("Entry\n");

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	retval = register_chrdev(EL7XX_MAJOR, "egis_fingerprint", &el7xx_fops);
	if (retval < 0) {
		pr_err("register_chrdev error.status:%d\n", retval);
		return retval;
	}

	el7xx_class = class_create(THIS_MODULE, "egis_fingerprint");
	if (IS_ERR(el7xx_class)) {
		pr_err("class_create error.\n");
		unregister_chrdev(EL7XX_MAJOR, el7xx_spi_driver.driver.name);
		return PTR_ERR(el7xx_class);
	}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	retval = spi_register_driver(&el7xx_spi_driver);
#else
	retval = platform_driver_register(&el7xx_spi_driver);
#endif
	if (retval < 0) {
		pr_err("register_driver error.\n");
		class_destroy(el7xx_class);
		unregister_chrdev(EL7XX_MAJOR, el7xx_spi_driver.driver.name);
		return retval;
	}

	pr_info("is successful\n");
	return retval;
}

static void __exit el7xx_exit(void)
{
	pr_info("Entry\n");

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	spi_unregister_driver(&el7xx_spi_driver);
#else
	platform_driver_unregister(&el7xx_spi_driver);
#endif
	class_destroy(el7xx_class);
	unregister_chrdev(EL7XX_MAJOR, el7xx_spi_driver.driver.name);
}

module_init(el7xx_init);
module_exit(el7xx_exit);

MODULE_AUTHOR("fp.sec@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Inc. EL7XX driver");
MODULE_LICENSE("GPL");
