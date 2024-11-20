// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include "panel_drv.h"
#include "panel_i2c.h"
#include "panel_debug.h"

#define PANEL_I2C_DRIVER_NAME "panel_i2c"

#define MAX_TX_SIZE 16
#define MAX_RX_SIZE 16

__visible_for_testing int panel_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	int ret = 0;

	if (!adap) {
		panel_err("invalid adap.\n");
		return -EINVAL;
	}

	if (!msgs) {
		panel_err("invalid msgs.\n");
		return -EINVAL;
	}

	if (num <= 0) {
		panel_err("invalid num(%d).\n", num);
		return -EINVAL;
	}

	ret = i2c_transfer(adap, msgs, num);

	return ret;
}

__visible_for_testing int panel_i2c_tx(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len)
{
	int ret = 0;
	int tx_len = 0;
	int tx_cnt = 0;
	int i = 0;
	u8 tx_buf[MAX_TX_SIZE] = {0, };

	struct i2c_msg msg[] = {
		{
			.addr = 0,
			.flags = 0,
			.len = tx_len,
			.buf = tx_buf,
		}
	};

	if (!i2c_dev) {
		panel_err("i2c_dev is null.\n");
		return -EINVAL;
	}

	if (!arr) {
		panel_err("arr is null.\n");
		return -EINVAL;
	}

	if (arr_len <= 0) {
		panel_err("arr_len is invalid(%d).\n", arr_len);
		return -EINVAL;
	}

	if (!i2c_dev->client) {
		panel_err("failed, i2c Client is NULL.\n");
		return -EINVAL;
	}

	tx_len = i2c_dev->addr_len + i2c_dev->data_len;
	if (tx_len > MAX_TX_SIZE) {
		panel_err("failed, tx_len is too long.(%d)\n", tx_len);
		return -EINVAL;
	}

	msg[0].addr = i2c_dev->client->addr;
	msg[0].len = tx_len;

	mutex_lock(&i2c_dev->lock);

	tx_cnt = arr_len / tx_len;

	if (arr_len % tx_len) {
		panel_err("there is rest. arr_size: %d rest: %d.\n",
				arr_len, arr_len % tx_len);
		ret = -EINVAL;
		goto tx_ret;
	}

	for (i = 0; i < tx_cnt; i++) {
		memcpy(tx_buf, arr + (i * tx_len), tx_len);

		ret = i2c_dev->ops->transfer(i2c_dev->client->adapter, msg, 1);

		if (unlikely(ret < 0)) {
			panel_err("i2c tx failed. [0x%02x]... : [0x%02x]... error %d\n",
					msg[0].buf[0], msg[0].buf[1], ret);
			goto tx_ret;
		}
	}

tx_ret:
	mutex_unlock(&i2c_dev->lock);

	return ret;
}

#define PANEL_I2C_RX_DUMP
__visible_for_testing int panel_i2c_rx(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len)
{
	int tx_len = 0;
	int rx_cnt = 0;
	int i = 0;
	int j = 0;
	int printout_pos;
	int ret = 0;
	char printout_buf[128] = {0, };
	u8 addr_buf[MAX_TX_SIZE] = {0, };
	u8 rx_buf[MAX_RX_SIZE] = {0, };

	struct i2c_msg msg[] = {
		{
			.addr = 0,
			.flags = 0,
			.len = 0,
			.buf = addr_buf,
		},
		{
			.addr = 0,
			.flags = I2C_M_RD,
			.len = 0,
			.buf = rx_buf,
		},
	};

	if (!i2c_dev) {
		panel_err("i2c_dev is null.\n");
		return -EINVAL;
	}

	if (!arr) {
		panel_err("arr is null.\n");
		return -EINVAL;
	}

	if (arr_len <= 0) {
		panel_err("arr_len is invalid(%d).\n", arr_len);
		return -EINVAL;
	}

	if (!i2c_dev->client) {
		panel_err("failed, i2c Client is NULL.\n");
		return -EINVAL;
	}

	tx_len = i2c_dev->addr_len + i2c_dev->data_len;
	if (tx_len > MAX_TX_SIZE) {
		panel_err("failed, tx_len is too long.(%d)\n", tx_len);
		return -EINVAL;
	}

	msg[0].addr = i2c_dev->client->addr;
	msg[1].addr = i2c_dev->client->addr;
	msg[0].len = i2c_dev->addr_len;
	msg[1].len = i2c_dev->data_len;

	mutex_lock(&i2c_dev->lock);

	rx_cnt = arr_len / tx_len;

	if (arr_len % tx_len) {
		panel_err("there is rest. arr_size: %d rest: %d.\n",
				arr_len, arr_len % tx_len);
		ret = -EINVAL;
		goto rx_ret;
	}

	for (i = 0; i < rx_cnt; i++) {
		memcpy(addr_buf, arr + (i * tx_len), i2c_dev->addr_len);
		memset(printout_buf, 0x00, ARRAY_SIZE(printout_buf));
		printout_pos = 0;

		ret = i2c_dev->ops->transfer(i2c_dev->client->adapter, msg, 2);
		if (unlikely(ret < 0)) {
			panel_err("i2c rx failed at [%d][0x%02x] error %d\n",
					i, msg[0].buf[0], ret);
			goto rx_ret;
		}

		/* addr * addr_len */
		for (j = 0; j < i2c_dev->addr_len; j++)
			printout_pos += snprintf(printout_buf + printout_pos,
						sizeof(printout_buf) - printout_pos, "[0x%02x]", addr_buf[j]);

		printout_pos += snprintf(printout_buf + printout_pos, sizeof(printout_buf) - printout_pos, ": ");

		/* rx_buf * data_len */
		for (j = 0; j < i2c_dev->data_len; j++)
			snprintf(printout_buf + printout_pos, sizeof(printout_buf) - printout_pos, "[0x%02x]", rx_buf[j]);

		memcpy(arr + (i * tx_len) + i2c_dev->addr_len, &rx_buf[0], i2c_dev->data_len);
#ifdef PANEL_I2C_RX_DUMP
		panel_info("%s\n", printout_buf);
#endif
	}
rx_ret:
	mutex_unlock(&i2c_dev->lock);

	return ret;
}

static struct i2c_drv_ops panel_i2c_drv_ops = {
	.tx = panel_i2c_tx,
	.rx = panel_i2c_rx,
	.transfer = panel_i2c_transfer,
};

__visible_for_testing int panel_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct panel_device *panel = NULL;
	struct panel_i2c_dev *panel_i2c_dev;
	int ret = 0;

	if (unlikely(!client)) {
		panel_err("invalid i2c.\n");
		return -EINVAL;
	}

	if (!id || !id->driver_data) {
		panel_err("invalid argument\n");
		return -EINVAL;
	}
	panel = (struct panel_device *)id->driver_data;

	if (panel->nr_i2c_dev >= PANEL_I2C_MAX) {
		panel_err("i2c_dev array is full. (%d)\n", panel->nr_i2c_dev);
		return -EINVAL;
	}

	panel_i2c_dev = kzalloc(sizeof(struct panel_i2c_dev), GFP_KERNEL);

	if (!panel_i2c_dev) {
		pr_err("i2c_dev is null\n");
		return -EINVAL;
	}

	panel_i2c_dev->client = client;
	panel_i2c_dev->reg = client->addr;
	panel_i2c_dev->ops = &panel_i2c_drv_ops;

	ret = of_property_read_u32(client->dev.of_node, "len,addr", &panel_i2c_dev->addr_len);
	ret = of_property_read_u32(client->dev.of_node, "len,data", &panel_i2c_dev->data_len);

	panel_i2c_dev->id = panel->nr_i2c_dev;

	memcpy(&panel->i2c_dev[panel->nr_i2c_dev], panel_i2c_dev, sizeof(struct panel_i2c_dev));
	mutex_init(&panel->i2c_dev[panel->nr_i2c_dev].lock);
	panel->nr_i2c_dev++;

	kfree(panel_i2c_dev);

	dev_info(&client->dev, "%s:(%s)(%s)(nr:%d)(addr:%d)\n", __func__, dev_name(&client->adapter->dev),
						of_node_full_name(client->dev.of_node),
						panel->nr_i2c_dev, client->addr);

	return ret;
}

static int panel_i2c_remove(struct i2c_client *client)
{
	return 0;
}


static struct i2c_device_id panel_i2c_id[] = {
	{"i2c", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, panel_i2c_id);

static const struct of_device_id panel_i2c_dt_ids[] = {
	{ .compatible = "panel_drv,i2c" },
	{ }
};

MODULE_DEVICE_TABLE(of, panel_i2c_dt_ids);

static struct i2c_driver panel_i2c_driver = {
	.driver = {
		.name		= PANEL_I2C_DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(panel_i2c_dt_ids),
	},
	.id_table		= panel_i2c_id,
	.probe		= panel_i2c_probe,
	.remove		= panel_i2c_remove,
};

int panel_i2c_drv_probe(struct panel_device *panel)
{
	int ret = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_i2c_id->driver_data = (kernel_ulong_t)panel;

	ret = i2c_add_driver(&panel_i2c_driver);
	if (ret) {
		panel_err("failed to register for i2c device\n");
		return -EINVAL;
	}

	return 0;
}

MODULE_DESCRIPTION("i2c driver for panel");
MODULE_LICENSE("GPL");
