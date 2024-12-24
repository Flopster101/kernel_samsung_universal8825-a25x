// SPDX-License-Identifier: GPL-2.0
/*
 * Goodix Touchscreen Driver
 * Copyright (C) 2020 - 2021 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include "goodix_ts_core.h"

#define GOODIX_TOOLS_NAME		"gtp_tools"
#define GOODIX_TOOLS_VER_MAJOR		1
#define GOODIX_TOOLS_VER_MINOR		0
static const u16 goodix_tools_ver = ((GOODIX_TOOLS_VER_MAJOR << 8) +
		(GOODIX_TOOLS_VER_MINOR));

#define GOODIX_TS_IOC_MAGIC		'G'
#define NEGLECT_SIZE_MASK		(~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

#define GTP_IRQ_ENABLE	_IO(GOODIX_TS_IOC_MAGIC, 0)
#define GTP_DEV_RESET	_IO(GOODIX_TS_IOC_MAGIC, 1)
#define GTP_SEND_COMMAND (_IOW(GOODIX_TS_IOC_MAGIC, 2, u8) & NEGLECT_SIZE_MASK)
#define GTP_SEND_CONFIG	(_IOW(GOODIX_TS_IOC_MAGIC, 3, u8) & NEGLECT_SIZE_MASK)
#define GTP_ASYNC_READ	(_IOR(GOODIX_TS_IOC_MAGIC, 4, u8) & NEGLECT_SIZE_MASK)
#define GTP_SYNC_READ	(_IOR(GOODIX_TS_IOC_MAGIC, 5, u8) & NEGLECT_SIZE_MASK)
#define GTP_ASYNC_WRITE	(_IOW(GOODIX_TS_IOC_MAGIC, 6, u8) & NEGLECT_SIZE_MASK)
#define GTP_READ_CONFIG	(_IOW(GOODIX_TS_IOC_MAGIC, 7, u8) & NEGLECT_SIZE_MASK)
#define GTP_ESD_ENABLE	_IO(GOODIX_TS_IOC_MAGIC, 8)
#define GTP_TOOLS_VER   (_IOR(GOODIX_TS_IOC_MAGIC, 9, u8) & NEGLECT_SIZE_MASK)
#define GTP_TOOLS_CTRL_SYNC (_IOW(GOODIX_TS_IOC_MAGIC, 10, u8) & NEGLECT_SIZE_MASK)

#define MAX_BUF_LENGTH		(16*1024)
#define IRQ_FALG		(0x01 << 2)

#define I2C_MSG_HEAD_LEN	20

/*
 * struct goodix_tools_dev - goodix tools device struct
 * @ts: The goodix_ts_data struct of ts driver
 * @ops_mode: represent device work mode
 * @rawdiffcmd: Set secondary device into rawdata mode
 * @normalcmd: Set secondary device into normal mode
 * @wq: Wait queue struct use in synchronous data read
 * @mutex: Protect goodix_tools_dev
 * @in_use: device in use
 */
struct goodix_tools_dev {
	struct goodix_ts_data *ts;
	struct list_head head;
	unsigned int ops_mode;
	struct goodix_ts_cmd rawdiffcmd, normalcmd;
	wait_queue_head_t wq;
	struct mutex mutex;
	atomic_t in_use;
	struct goodix_ext_module module;
} *goodix_tools_dev;


/* read data asynchronous,
 * success return data length, otherwise return < 0
 */
static int async_read(struct goodix_tools_dev *dev, void __user *arg)
{
	u8 *databuf = NULL;
	int ret = 0;
	u32 reg_addr, length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];
	const struct goodix_ts_hw_ops *hw_ops = dev->ts->hw_ops;

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret)
		return -EFAULT;

	reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
		+ (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
		+ (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);
	if (length > MAX_BUF_LENGTH) {
		ts_err("buffer too long:%d > %d", length, MAX_BUF_LENGTH);
		return -EINVAL;
	}
	databuf = kzalloc(length, GFP_KERNEL);
	if (!databuf) {
		ts_err("Alloc memory failed");
		return -ENOMEM;
	}

	if (hw_ops->read(dev->ts, reg_addr, databuf, length)) {
		ret = -EBUSY;
		ts_err("Read i2c failed");
		goto err_out;
	}
	ret = copy_to_user((u8 *)arg + I2C_MSG_HEAD_LEN, databuf, length);
	if (ret) {
		ret = -EFAULT;
		ts_err("Copy_to_user failed");
		goto err_out;
	}
	ret = length;
err_out:
	kfree(databuf);
	return ret;
}

/* if success return config data length */
static int read_config_data(struct goodix_ts_data *ts, void __user *arg)
{
	int ret = 0;
	u32 reg_addr, length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];
	u8 *tmp_buf;

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}
	reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
		+ (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
		+ (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);
	ts_info("read config,reg_addr=0x%x, length=%d", reg_addr, length);
	if (length > MAX_BUF_LENGTH) {
		ts_err("buffer too long:%d > %d", length, MAX_BUF_LENGTH);
		return -EINVAL;
	}
	tmp_buf = kzalloc(length, GFP_KERNEL);
	if (!tmp_buf) {
		ts_err("failed alloc memory");
		return -ENOMEM;
	}
	/* if reg_addr == 0, read config data with specific flow */
	if (!reg_addr) {
		if (ts->hw_ops->read_config)
			ret = ts->hw_ops->read_config(ts, tmp_buf, length);
		else
			ret = -EINVAL;
	} else {
		ret = ts->hw_ops->read(ts, reg_addr, tmp_buf, length);
		if (!ret)
			ret = length;
	}
	if (ret <= 0)
		goto err_out;

	if (copy_to_user((u8 *)arg + I2C_MSG_HEAD_LEN, tmp_buf, ret)) {
		ret = -EFAULT;
		ts_err("Copy_to_user failed");
	}

err_out:
	kfree(tmp_buf);
	return ret;
}

/* write data to i2c asynchronous,
 * success return bytes write, else return <= 0
 */
static int async_write(struct goodix_tools_dev *dev, void __user *arg)
{
	u8 *databuf;
	int ret = 0;
	u32 reg_addr, length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN];
	struct goodix_ts_data *ts = dev->ts;
	const struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}
	reg_addr = i2c_msg_head[0] + (i2c_msg_head[1] << 8)
		+ (i2c_msg_head[2] << 16) + (i2c_msg_head[3] << 24);
	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
		+ (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);
	if (length > MAX_BUF_LENGTH) {
		ts_err("buffer too long:%d > %d", length, MAX_BUF_LENGTH);
		return -EINVAL;
	}

	databuf = kzalloc(length, GFP_KERNEL);
	if (!databuf) {
		ts_err("Alloc memory failed");
		return -ENOMEM;
	}
	ret = copy_from_user(databuf, (u8 *)arg + I2C_MSG_HEAD_LEN, length);
	if (ret) {
		ret = -EFAULT;
		ts_err("Copy data from user failed");
		goto err_out;
	}

	if (hw_ops->write(ts, reg_addr, databuf, length)) {
		ret = -EBUSY;
		ts_err("Write data to device failed");
	} else {
		ret = length;
	}

err_out:
	kfree(databuf);
	return ret;
}

static int init_cfg_data(struct goodix_ic_config *cfg, void __user *arg)
{
	int ret = 0;
	u32 length;
	u8 i2c_msg_head[I2C_MSG_HEAD_LEN] = {0};

	ret = copy_from_user(&i2c_msg_head, arg, I2C_MSG_HEAD_LEN);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}

	length = i2c_msg_head[4] + (i2c_msg_head[5] << 8)
		+ (i2c_msg_head[6] << 16) + (i2c_msg_head[7] << 24);
	if (length > GOODIX_CFG_MAX_SIZE) {
		ts_err("buffer too long:%d > %d", length, MAX_BUF_LENGTH);
		return -EINVAL;
	}
	ret = copy_from_user(cfg->data, (u8 *)arg + I2C_MSG_HEAD_LEN, length);
	if (ret) {
		ts_err("Copy data from user failed");
		return -EFAULT;
	}
	cfg->len = length;
	return 0;
}

/**
 * goodix_tools_ioctl - ioctl implementation
 *
 * @filp: Pointer to file opened
 * @cmd: Ioctl opertion command
 * @arg: Command data
 * Returns >=0 - succeed, else failed
 */
static long goodix_tools_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	struct goodix_tools_dev *dev = filp->private_data;
	struct goodix_ts_data *ts;
	const struct goodix_ts_hw_ops *hw_ops;
	struct goodix_ic_config *temp_cfg = NULL;

	if (dev->ts == NULL) {
		ts_err("Tools module not register");
		return -EINVAL;
	}
	ts = dev->ts;
	hw_ops = ts->hw_ops;

	if (_IOC_TYPE(cmd) != GOODIX_TS_IOC_MAGIC) {
		ts_err("Bad magic num:%c", _IOC_TYPE(cmd));
		return -ENOTTY;
	}

	switch (cmd & NEGLECT_SIZE_MASK) {
	case GTP_IRQ_ENABLE:
		if (arg == 1) {
			hw_ops->irq_enable(ts, true);
			mutex_lock(&dev->mutex);
			dev->ops_mode |= IRQ_FALG;
			mutex_unlock(&dev->mutex);
			ts_info("IRQ enabled");
		} else if (arg == 0) {
			hw_ops->irq_enable(ts, false);
			mutex_lock(&dev->mutex);
			dev->ops_mode &= ~IRQ_FALG;
			mutex_unlock(&dev->mutex);
			ts_info("IRQ disabled");
		} else {
			ts_info("Irq aready set with, arg = %ld", arg);
		}
		ret = 0;
		break;
	case GTP_ESD_ENABLE:
		if (arg == 0)
			goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
		else
			goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
		break;
	case GTP_DEV_RESET:
		hw_ops->reset(ts, GOODIX_NORMAL_RESET_DELAY_MS);
		break;
	case GTP_SEND_COMMAND:
		/* deprecated command */
		ts_err("the GTP_SEND_COMMAND function has been removed");
		ret = -EINVAL;
		break;
	case GTP_SEND_CONFIG:
		temp_cfg = kzalloc(sizeof(struct goodix_ic_config), GFP_KERNEL);
		if (temp_cfg == NULL) {
			ts_err("Memory allco err");
			ret = -ENOMEM;
			goto err_out;
		}

		ret = init_cfg_data(temp_cfg, (void __user *)arg);
		if (!ret && hw_ops->send_config) {
			ret = hw_ops->send_config(ts, temp_cfg->data, temp_cfg->len);
			if (ret) {
				ts_err("Failed send config");
				ret = -EAGAIN;
			} else {
				ts_info("Send config success");
				ret = 0;
			}
		}
		kfree(temp_cfg);
		temp_cfg = NULL;
		break;
	case GTP_READ_CONFIG:
		ret = read_config_data(ts, (void __user *)arg);
		if (ret > 0)
			ts_info("success read config:len=%d", ret);
		else
			ts_err("failed read config:ret=0x%x", ret);
		break;
	case GTP_ASYNC_READ:
		ret = async_read(dev, (void __user *)arg);
		if (ret < 0)
			ts_err("Async data read failed");
		break;
	case GTP_SYNC_READ:
		ts_info("unsupport sync read");
		break;
	case GTP_ASYNC_WRITE:
		ret = async_write(dev, (void __user *)arg);
		if (ret < 0)
			ts_err("Async data write failed");
		break;
	case GTP_TOOLS_VER:
		ret = copy_to_user((u8 *)arg, &goodix_tools_ver,
				sizeof(u16));
		if (ret)
			ts_err("failed copy driver version info to user");
		break;
	case GTP_TOOLS_CTRL_SYNC:
		ts->tools_ctrl_sync = !!arg;
		ts_info("set tools ctrl sync %d", ts->tools_ctrl_sync);
		break;
	default:
		ts_info("Invalid cmd");
		ret = -ENOTTY;
		break;
	}

err_out:
	return ret;
}

#ifdef CONFIG_COMPAT
static long goodix_tools_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)arg32);
}
#endif

static int goodix_tools_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	ts_info("try open tool");
	/* Only the first time open device need to register module */
	ret = goodix_register_ext_module_no_wait(&goodix_tools_dev->module);
	if (ret) {
		ts_info("failed register to core module");
		return -EFAULT;
	}
	ts_info("success open tools");
	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	filp->private_data = goodix_tools_dev;
	atomic_set(&goodix_tools_dev->in_use, 1);
	return 0;
}

static int goodix_tools_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	/* when the last close this dev node unregister the module */
	goodix_tools_dev->ts->tools_ctrl_sync = false;
	atomic_set(&goodix_tools_dev->in_use, 0);
	goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	ret = goodix_unregister_ext_module(&goodix_tools_dev->module);
	return ret;
}

static int goodix_tools_module_init(struct goodix_ts_data *core_data,
		struct goodix_ext_module *module)
{
	struct goodix_tools_dev *tools_dev = module->priv_data;

	if (core_data)
		tools_dev->ts = core_data;
	else
		return -ENODEV;

	return 0;
}

static int goodix_tools_module_exit(struct goodix_ts_data *core_data,
		struct goodix_ext_module *module)
{
	struct goodix_tools_dev *tools_dev = module->priv_data;

	ts_debug("tools module unregister");
	if (atomic_read(&tools_dev->in_use)) {
		ts_err("tools module busy, please close it then retry");
		return -EBUSY;
	}
	return 0;
}

static const struct file_operations goodix_tools_fops = {
	.owner		= THIS_MODULE,
	.open		= goodix_tools_open,
	.release	= goodix_tools_release,
	.unlocked_ioctl	= goodix_tools_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = goodix_tools_compat_ioctl,
#endif
};

static struct miscdevice goodix_tools_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= GOODIX_TOOLS_NAME,
	.fops	= &goodix_tools_fops,
};

static struct goodix_ext_module_funcs goodix_tools_module_funcs = {
	.init = goodix_tools_module_init,
	.exit = goodix_tools_module_exit,
};

/**
 * goodix_tools_init - init goodix tools device and register a miscdevice
 *
 * return: 0 success, else failed
 */
int goodix_tools_init(void)
{
	int ret;

	goodix_tools_dev = kzalloc(sizeof(struct goodix_tools_dev), GFP_KERNEL);
	if (goodix_tools_dev == NULL) {
		ts_err("Memory allco err");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&goodix_tools_dev->head);
	goodix_tools_dev->ops_mode = 0;
	goodix_tools_dev->ops_mode |= IRQ_FALG;
	init_waitqueue_head(&goodix_tools_dev->wq);
	mutex_init(&goodix_tools_dev->mutex);
	atomic_set(&goodix_tools_dev->in_use, 0);

	goodix_tools_dev->module.funcs = &goodix_tools_module_funcs;
	goodix_tools_dev->module.name = GOODIX_TOOLS_NAME;
	goodix_tools_dev->module.priv_data = goodix_tools_dev;
	goodix_tools_dev->module.priority = EXTMOD_PRIO_DBGTOOL;

	ret = misc_register(&goodix_tools_miscdev);
	if (ret)
		ts_err("Debug tools miscdev register failed");
	else
		ts_info("Debug tools miscdev register success");

	return ret;
}

void goodix_tools_exit(void)
{
	misc_deregister(&goodix_tools_miscdev);
	kfree(goodix_tools_dev);
	ts_info("Debug tools miscdev exit");
}

/* show driver information */
static ssize_t driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Goodix ts driver for SEC:%s\n",
			GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
//#define NORMANDY_CFG_VER_REG	0x6F78
static ssize_t chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	struct goodix_fw_version chip_ver;
	u8 temp_pid[8] = {0};
	int ret;
	int cnt = 0;

	ret = hw_ops->read_version(ts, &chip_ver);
	if (!ret) {
		memcpy(temp_pid, chip_ver.rom_pid, sizeof(chip_ver.rom_pid));
		cnt += snprintf(&buf[0], PAGE_SIZE,
				"rom_pid:%s\nrom_vid:%02x%02x%02x\n",
				temp_pid, chip_ver.rom_vid[0],
				chip_ver.rom_vid[1], chip_ver.rom_vid[2]);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"patch_pid:%s\npatch_vid:%02x%02x%02x%02x\n",
				chip_ver.patch_pid, chip_ver.patch_vid[0],
				chip_ver.patch_vid[1], chip_ver.patch_vid[2],
				chip_ver.patch_vid[3]);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"sensorid:%d\n", chip_ver.sensor_id);
	}

	ret = hw_ops->get_ic_info(ts, &ts->ic_info);
	if (!ret) {
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"config_id:%x\n", ts->ic_info.version.config_id);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"config_version:%x\n", ts->ic_info.version.config_version);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
			"firmware_checksum:%x\n", ts->ic_info.sec.total_checksum);
	}

	return cnt;
}

/* reset chip */
static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0')
		hw_ops->reset(ts, GOODIX_NORMAL_RESET_DELAY_MS);
	return count;
}

/* read config */
static ssize_t read_cfg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	int ret;
	int i;
	int offset;
	char *cfg_buf = NULL;

	cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cfg_buf)
		return -ENOMEM;

	ret = hw_ops->read_config(ts, cfg_buf, PAGE_SIZE);
	if (ret > 0) {
		offset = 0;
		for (i = 0; i < 200; i++) { // only print 200 bytes
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					"%02x,", cfg_buf[i]);
			if ((i + 1) % 20 == 0)
				buf[offset++] = '\n';
		}
	}

	kfree(cfg_buf);
	if (ret <= 0)
		return ret;

	return offset;
}

static u8 ascii2hex(u8 a)
{
	s8 value = 0;

	if (a >= '0' && a <= '9')
		value = a - '0';
	else if (a >= 'A' && a <= 'F')
		value = a - 'A' + 0x0A;
	else if (a >= 'a' && a <= 'f')
		value = a - 'a' + 0x0A;
	else
		value = 0xff;

	return value;
}

static int goodix_ts_convert_0x_data(const u8 *buf, int buf_size,
		u8 *out_buf, int *out_buf_len)
{
	int i, m_size = 0;
	int temp_index = 0;
	u8 high, low;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X')
			m_size++;
	}

	if (m_size <= 1) {
		ts_err("cfg file ERROR, valid data count:%d", m_size);
		return -EINVAL;
	}
	*out_buf_len = m_size;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] != 'x' && buf[i] != 'X')
			continue;

		if (temp_index >= m_size) {
			ts_err("exchange cfg data error, overflow, temp_index:%d,m_size:%d",
					temp_index, m_size);
			return -EINVAL;
		}
		high = ascii2hex(buf[i + 1]);
		low = ascii2hex(buf[i + 2]);
		if (high == 0xff || low == 0xff) {
			ts_err("failed convert: 0x%x, 0x%x",
					buf[i + 1], buf[i + 2]);
			return -EINVAL;
		}
		out_buf[temp_index++] = (high << 4) + low;
	}
	return 0;
}

/* send config */
static ssize_t send_cfg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	struct goodix_ic_config *config = NULL;
	const struct firmware *cfg_img = NULL;
	int en;
	int ret;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	hw_ops->irq_enable(ts, false);

	ret = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
	if (ret < 0) {
		ts_err("cfg file [%s] not available,errno:%d",
				GOODIX_DEFAULT_CFG_NAME, ret);
		goto exit;
	} else {
		ts_info("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);
	}

	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		goto exit;

	if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
				config->data, &config->len)) {
		ts_err("convert config data FAILED");
		goto exit;
	}

	ret = hw_ops->send_config(ts, config->data, config->len);
	if (ret < 0)
		ts_err("send config failed");


exit:
	hw_ops->irq_enable(ts, true);
	kfree(config);
	if (cfg_img)
		release_firmware(cfg_img);

	return count;
}

/* reg read/write */
static u32 rw_addr;
static u32 rw_len;
static u8 rw_flag;
static u8 store_buf[32];
static u8 show_buf[PAGE_SIZE];
static ssize_t reg_rw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	int ret;

	if (!rw_addr || !rw_len) {
		ts_err("address(0x%x) and length(%d) can't be null",
				rw_addr, rw_len);
		return -EINVAL;
	}

	if (rw_flag != 1) {
		ts_err("invalid rw flag %d, only support [1/2]", rw_flag);
		return -EINVAL;
	}

	ret = hw_ops->read(ts, rw_addr, show_buf, rw_len);
	if (ret < 0) {
		ts_err("failed read addr(%x) length(%d)", rw_addr, rw_len);
		return snprintf(buf, PAGE_SIZE, "failed read addr(%x), len(%d)\n",
				rw_addr, rw_len);
	}

	return snprintf(buf, PAGE_SIZE, "0x%x,%d {%*ph}\n",
			rw_addr, rw_len, rw_len, show_buf);
}

static ssize_t reg_rw_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	char *pos = NULL;
	char *token = NULL;
	long result = 0;
	int ret;
	int i;

	if (!buf || !count) {
		ts_err("invalid parame");
		goto err_out;
	}

	if (buf[0] == 'r') {
		rw_flag = 1;
	} else if (buf[0] == 'w') {
		rw_flag = 2;
	} else {
		ts_err("string must start with 'r/w'");
		goto err_out;
	}

	/* get addr */
	pos = (char *)buf;
	pos += 2;
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid address info");
		goto err_out;
	} else {
		if (kstrtol(token, 16, &result)) {
			ts_err("failed get addr info");
			goto err_out;
		}
		rw_addr = (u32)result;
		ts_info("rw addr is 0x%x", rw_addr);
	}

	/* get length */
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid length info");
		goto err_out;
	} else {
		if (kstrtol(token, 0, &result)) {
			ts_err("failed get length info");
			goto err_out;
		}
		rw_len = (u32)result;
		ts_info("rw length info is %d", rw_len);
		if (rw_len > sizeof(store_buf)) {
			ts_err("data len > %lu", sizeof(store_buf));
			goto err_out;
		}
	}

	if (rw_flag == 1)
		return count;

	for (i = 0; i < rw_len; i++) {
		token = strsep(&pos, ":");
		if (!token) {
			ts_err("invalid data info");
			goto err_out;
		} else {
			if (kstrtol(token, 16, &result)) {
				ts_err("failed get data[%d] info", i);
				goto err_out;
			}
			store_buf[i] = (u8)result;
			ts_info("get data[%d]=0x%x", i, store_buf[i]);
		}
	}
	ret = hw_ops->write(ts, rw_addr, store_buf, rw_len);
	if (ret < 0) {
		ts_err("failed write addr(%x) data %*ph", rw_addr,
				rw_len, store_buf);
		goto err_out;
	}

	ts_info("%s write to addr (%x) with data %*ph",
			"success", rw_addr, rw_len, store_buf);

	return count;
err_out:
	snprintf(show_buf, PAGE_SIZE, "%s\n",
			"invalid params, format{r/w:4100:length:[41:21:31]}");
	return -EINVAL;

}

/* show irq information */
static ssize_t irq_info_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct irq_desc *desc;
	size_t offset = 0;
	int r;

	r = snprintf(&buf[offset], PAGE_SIZE, "irq:%u\n", ts->irq);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "state:%s\n",
			(atomic_read(&ts->plat_data->irq_enabled) == SEC_INPUT_IRQ_ENABLE) ?
			"enabled" : "disabled");
	if (r < 0)
		return -EINVAL;

	desc = irq_to_desc(ts->irq);
	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "disable-depth:%d\n",
			desc->depth);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "trigger-count:%zu\n",
			ts->irq_trig_cnt);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset,
			"echo 0/1 > irq_info to disable/enable irq\n");
	if (r < 0)
		return -EINVAL;

	offset += r;
	return offset;
}

/* enable/disable irq */
static ssize_t irq_info_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		hw_ops->irq_enable(ts, true);
	else
		hw_ops->irq_enable(ts, false);
	return count;
}

/* show esd status */
static ssize_t esd_info_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
			atomic_read(&ts_esd->esd_on) ?
			"enabled" : "disabled");

	return r;
}

/* enable/disable esd */
static ssize_t esd_info_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	else
		goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	return count;
}

/* debug level show */
static ssize_t debug_log_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
			debug_log_flag ?
			"enabled" : "disabled");

	return r;
}

/* debug level store */
static ssize_t debug_log_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		debug_log_flag = true;
	else
		debug_log_flag = false;
	return count;
}

static DEVICE_ATTR_RO(driver_info);
static DEVICE_ATTR_RO(chip_info);
static DEVICE_ATTR_WO(reset);
static DEVICE_ATTR_WO(send_cfg);
static DEVICE_ATTR_RO(read_cfg);
static DEVICE_ATTR_RW(reg_rw);
static DEVICE_ATTR_RW(irq_info);
static DEVICE_ATTR_RW(esd_info);
static DEVICE_ATTR_RW(debug_log);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_reg_rw.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_esd_info.attr,
	&dev_attr_debug_log.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

int goodix_ts_sysfs_init(struct goodix_ts_data *ts)
{
	int ret;

	ret = sysfs_create_group(&ts->pdev->dev.kobj, &sysfs_group);
	if (ret) {
		ts_err("failed create core sysfs group");
		return ret;
	}

	return ret;
}

void goodix_ts_sysfs_exit(struct goodix_ts_data *ts)
{
	sysfs_remove_group(&ts->pdev->dev.kobj, &sysfs_group);
}

/* prosfs create */
static int rawdata_proc_show(struct seq_file *m, void *v)
{
	struct ts_rawdata_info *info;
	struct goodix_ts_data *ts;
	int tx;
	int rx;
	int ret;
	int i;
	int index;

	if (!m || !v) {
		ts_err("%s : input null ptr", __func__);
		return -EIO;
	}

	ts = m->private;
	if (!ts) {
		ts_err("can't get core data");
		return -EIO;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory");
		return -ENOMEM;
	}

	ret = ts->hw_ops->get_capacitance_data(ts, info);
	if (ret < 0) {
		ts_err("failed to get_capacitance_data, exit!");
		goto exit;
	}

	rx = info->buff[0];
	tx = info->buff[1];
	seq_printf(m, "TX:%d  RX:%d\n", tx, rx);
	seq_puts(m, "mutual_rawdata:\n");
	index = 2;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%5d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}
	seq_puts(m, "mutual_diffdata:\n");
	index += tx * rx;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%3d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}

exit:
	kfree(info);
	return ret;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	return single_open_size(file, rawdata_proc_show, pde_data(inode), PAGE_SIZE * 10);
#else
	return single_open_size(file, rawdata_proc_show, PDE_DATA(inode), PAGE_SIZE * 10);
#endif
}

#if (KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE)
static const struct proc_ops rawdata_proc_fops = {
	.proc_open = rawdata_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations rawdata_proc_fops = {
	.open = rawdata_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

void goodix_ts_procfs_init(struct goodix_ts_data *ts)
{
	struct proc_dir_entry *proc_entry;

	if (!proc_mkdir("goodix_ts", NULL))
		return;

	proc_entry = proc_create_data("goodix_ts/tp_capacitance_data",
			0664, NULL, &rawdata_proc_fops, ts);
	if (!proc_entry)
		ts_err("failed to create proc entry");
}

void goodix_ts_procfs_exit(struct goodix_ts_data *ts)
{
	remove_proc_entry("goodix_ts/tp_capacitance_data", NULL);
	remove_proc_entry("goodix_ts", NULL);
}