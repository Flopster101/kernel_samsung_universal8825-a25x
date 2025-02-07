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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#include "goodix_ts_core.h"

#define TS_DRIVER_NAME				"goodix_i2c"
#define I2C_MAX_TRANSFER_SIZE		256
#define GOODIX_BUS_RETRY_TIMES		3
#define BERLIN_REG_ADDR_SIZE		4
#define NORMANDY_REG_ADDR_SIZE		2

static struct platform_device *goodix_pdev;
struct goodix_bus_interface goodix_i2c_bus;

/* Berlin read/write ops */
static int goodix_i2c_read(struct device *dev, unsigned int reg,
		unsigned char *data, size_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	unsigned int transfer_length = 0;
	unsigned int pos = 0, address = reg;
	unsigned char get_buf[128], addr_buf[BERLIN_REG_ADDR_SIZE];
	int retry, r = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = BERLIN_REG_ADDR_SIZE,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
		}
	};

	if (!ts)
		return -ENODEV;

	if (ts->plat_data->power_enabled == false) {
		ts_err("IC power is off");
		return -EIO;
	}

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &client->dev, "%s: shutdown was called\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (!ts->plat_data->resume_done.done) {
		int ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done,
				msecs_to_jiffies(SEC_TS_WAKE_LOCK_TIME));

		if (ret <= 0) {
			ts_err("LPM: pm resume is not handled:%d", ret);
			return -EIO;
		}
	}

	if (likely(len < sizeof(get_buf))) {
		/* code optimize, use stack memory */
		msgs[1].buf = &get_buf[0];
	} else {
		msgs[1].buf = kzalloc(len, GFP_KERNEL);
		if (msgs[1].buf == NULL)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE))
			transfer_length = I2C_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;

		msgs[0].buf[0] = (address >> 24) & 0xFF;
		msgs[0].buf[1] = (address >> 16) & 0xFF;
		msgs[0].buf[2] = (address >> 8) & 0xFF;
		msgs[0].buf[3] = address & 0xFF;
		msgs[1].len = transfer_length;

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
							msgs, 2) == 2)) {
				memcpy(&data[pos], msgs[1].buf, transfer_length);

				if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
					int i;
					char *dbuff;
					char dtemp[4] = {0};
					int dbuff_len = (3 * transfer_length) + 1;

					dbuff = kzalloc(dbuff_len, GFP_KERNEL);
					if (!dbuff) {
						r = -EAGAIN;
						goto read_exit;
					}

					for (i = 0; i < transfer_length; i++) {
						snprintf(dtemp, sizeof(dtemp), "%02X ", data[pos + i]);
						strlcat(dbuff, dtemp, dbuff_len);
					}
					ts_info("sec_input : i2c_cmd: R: lenth(%d) pos(%d) 0x%02X | %s", transfer_length, pos, reg, dbuff);
					kfree(dbuff);
				}

				pos += transfer_length;
				address += transfer_length;

				break;
			}
			ts_info("I2c read retry[%d]:0x%x", retry + 1, reg);
			ts->plat_data->hw_param.comm_err_count++;
			sec_delay(20);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_err("I2c read failed,dev:%02x,reg:%04x,size:%ld",
					client->addr, reg, len);
			r = -EAGAIN;
			goto read_exit;
		}
	}

read_exit:
	if (unlikely(len >= sizeof(get_buf)))
		kfree(msgs[1].buf);
	return r;
}

static int goodix_i2c_write(struct device *dev, unsigned int reg,
		unsigned char *data, size_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	unsigned int pos = 0, transfer_length = 0;
	unsigned int address = reg;
	unsigned char put_buf[128];
	int retry, r = 0;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = !I2C_M_RD,
	};

	if (!ts)
		return -ENODEV;

	if (ts->plat_data->power_enabled == false) {
		ts_err("IC power is off");
		return -EIO;
	}

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &client->dev, "%s: shutdown was called\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (!ts->plat_data->resume_done.done) {
		int ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done,
				msecs_to_jiffies(SEC_TS_WAKE_LOCK_TIME));

		if (ret <= 0) {
			ts_err("LPM: pm resume is not handled:%d", ret);
			return -EIO;
		}
	}

	if (likely(len + BERLIN_REG_ADDR_SIZE < sizeof(put_buf))) {
		/* code optimize,use stack memory*/
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(len + BERLIN_REG_ADDR_SIZE, GFP_KERNEL);
		if (msg.buf == NULL)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE -
					BERLIN_REG_ADDR_SIZE))
			transfer_length = I2C_MAX_TRANSFER_SIZE -
				BERLIN_REG_ADDR_SIZE;
		else
			transfer_length = len - pos;
		msg.buf[0] = (address >> 24) & 0xFF;
		msg.buf[1] = (address >> 16) & 0xFF;
		msg.buf[2] = (address >> 8) & 0xFF;
		msg.buf[3] = address & 0xFF;

		msg.len = transfer_length + BERLIN_REG_ADDR_SIZE;
		memcpy(&msg.buf[BERLIN_REG_ADDR_SIZE],
				&data[pos], transfer_length);

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
							&msg, 1) == 1)) {
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_debug("I2c write retry[%d]", retry + 1);
			ts->plat_data->hw_param.comm_err_count++;
			sec_delay(20);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_err("I2c write failed,dev:%02x,reg:%04x,size:%ld",
					client->addr, reg, len);
			r = -EAGAIN;
			goto write_exit;
		}
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;
		char *dbuff;
		char dtemp[4] = { 0 };
		int dbuff_len = (3 * len) + 1;

		dbuff = kzalloc(dbuff_len, GFP_KERNEL);
		if (!dbuff)
			goto write_exit;

		for (i = 0; i < len; i++) {
			snprintf(dtemp, sizeof(dtemp), "%02X ", data[i]);
			strlcat(dbuff, dtemp, dbuff_len);
		}
		ts_info("sec_input : i2c_cmd: W: 0x%02X | %s", reg, dbuff);
		kfree(dbuff);
	}

write_exit:
	if (likely(len + BERLIN_REG_ADDR_SIZE >= sizeof(put_buf)))
		kfree(msg.buf);
	return r;
}

static void goodix_pdev_release(struct device *dev)
{
	ts_info("goodix pdev released");
	kfree(goodix_pdev);
}

static int goodix_i2c_init(struct i2c_client *client)
{
	struct goodix_ts_data *ts = NULL;
	struct sec_ts_plat_data *pdata;
	int ret = 0;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			ret = -ENOMEM;
			goto error_allocate_pdata;
		}

		client->dev.platform_data = pdata;

		ret = sec_input_parse_dt(&client->dev);
		if (ret) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			ret = -ENOMEM;
			input_err(true, &client->dev, "%s: No platform data found\n", __func__);
			goto error_allocate_pdata;
		}
	}

	ts = devm_kzalloc(&client->dev,
			sizeof(struct goodix_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		input_err(true, &client->dev, "%s: Failed to allocate memory for core data\n", __func__);
		goto error_allocate_mem;
	}

	i2c_set_clientdata(client, ts);

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl))
		input_err(true, &client->dev, "%s: could not get pinctrl\n", __func__);

	ptsp = &client->dev;

	input_info(true, &client->dev, "%s: init resource\n", __func__);

	return 0;

error_allocate_mem:
error_allocate_pdata:
	input_err(true, &client->dev, "%s: failed(%d)\n", __func__, ret);
	input_log_fix();
	return ret;
}

#if (KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE)
static int goodix_i2c_probe(struct i2c_client *client)
#else
static int goodix_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
#endif
{
	int ret = 0;

	ts_info("goodix i2c probe in 2024");
	ret = i2c_check_functionality(client->adapter,
			I2C_FUNC_I2C);
	if (!ret)
		return -EIO;

	ret = goodix_i2c_init(client);
	if (ret < 0) {
		ts_err("%s: fail to init resource\n", __func__);
		return ret;
	}

	goodix_i2c_bus.bus_type = GOODIX_BUS_TYPE_I2C;
	goodix_i2c_bus.dev = &client->dev;
	goodix_i2c_bus.read = goodix_i2c_read;
	goodix_i2c_bus.write = goodix_i2c_write;
	/* ts core device */
	goodix_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (!goodix_pdev)
		return -ENOMEM;

	goodix_pdev->name = GOODIX_CORE_DRIVER_NAME;
	goodix_pdev->id = 0;
	goodix_pdev->num_resources = 0;
	/*
	 * you can find this platform dev in
	 * /sys/devices/platform/goodix_ts.0
	 * goodix_pdev->dev.parent = &client->dev;
	 */
	goodix_pdev->dev.platform_data = &goodix_i2c_bus;
	goodix_pdev->dev.release = goodix_pdev_release;

	/* register platform device, then the goodix_ts_core
	 * module will probe the touch device.
	 */
	ret = platform_device_register(goodix_pdev);
	if (ret) {
		ts_err("failed register goodix platform device, %d", ret);
		goto err_pdev;
	}
	ts_info("i2c probe out");
	return ret;

err_pdev:
	kfree(goodix_pdev);
	goodix_pdev = NULL;
	ts_info("i2c probe out, %d", ret);
	return ret;
}

static int goodix_i2c_dev_remove(struct i2c_client *client)
{
	if (!goodix_pdev)
		return 0;

	ts_info("called");
	platform_device_unregister(goodix_pdev);
	goodix_pdev = NULL;
	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void goodix_i2c_remove(struct i2c_client *client)
{
	goodix_i2c_dev_remove(client);
}
#else
static int goodix_i2c_remove(struct i2c_client *client)
{
	goodix_i2c_dev_remove(client);
	return 0;
}
#endif

static void goodix_i2c_shutdown(struct i2c_client *client)
{
	struct sec_ts_plat_data *pdata = client->dev.platform_data;

	ts_info("called");
	atomic_set(&pdata->shutdown_called, true);
	goodix_i2c_remove(client);
}

static const struct of_device_id i2c_matches[] = {
	{.compatible = "goodix,berlin",},
	{.compatible = "goodix,gt9897",},
	{.compatible = "goodix,gt9966",},
	{.compatible = "goodix,gt9916",},
	{.compatible = "goodix,gt9885",},
	{.compatible = "goodix,gt9886",},
	{},
};
MODULE_DEVICE_TABLE(of, i2c_matches);

static const struct i2c_device_id i2c_id_table[] = {
	{TS_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, i2c_id_table);

static struct i2c_driver goodix_i2c_driver = {
	.driver = {
		.name = TS_DRIVER_NAME,
		//.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(i2c_matches),
	},
	.probe = goodix_i2c_probe,
	.remove = goodix_i2c_remove,
	.shutdown = goodix_i2c_shutdown,
	.id_table = i2c_id_table,
};

int goodix_i2c_bus_init(void)
{
	ts_info("Goodix i2c driver init");
	return i2c_add_driver(&goodix_i2c_driver);
}

void goodix_i2c_bus_exit(void)
{
	ts_info("Goodix i2c driver exit");
	i2c_del_driver(&goodix_i2c_driver);
}
