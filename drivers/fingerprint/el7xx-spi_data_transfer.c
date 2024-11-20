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

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "el7xx.h"

#ifndef ENABLE_SENSORS_FPRINT_SECURE
int el7xx_spi_setup_conf(struct el7xx_data *etspi, u32 bits)
{
	int retval = 0;

	if (bits < 1 || bits > 4) {
		pr_err("Invalid argument. bits(%d) > 4\n", bits);
		return -EINVAL;
	}

	etspi->spi->bits_per_word = 8 * bits;
	if (etspi->prev_bits_per_word != etspi->spi->bits_per_word) {
		retval = spi_setup(etspi->spi);
		if (retval != 0) {
			pr_err("spi_setup() is failed. status : %d\n", retval);
			return retval;
		}

		pr_info("prev-bpw:%d, bpw:%d\n",
				etspi->prev_bits_per_word, etspi->spi->bits_per_word);
		etspi->prev_bits_per_word = etspi->spi->bits_per_word;
	}

	return retval;
}

int el7xx_spi_sync(struct el7xx_data *etspi, int len)
{
	int retval = 0;

	struct spi_message	m;
	struct spi_transfer xfer = {
		.tx_buf		= etspi->buf,
		.rx_buf		= etspi->buf,
		.len		= len,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0)
		pr_info("error retval = %d\n", retval);

	return retval;
}

int el7xx_io_read_register(struct el7xx_data *etspi, u8 *addr, u8 *buf, struct egis_ioc_transfer *ioc)
{
	int retval = 0;
	int read_len = 1;
	int spi_len = ioc->len + 2;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_R;
	if (copy_from_user(etspi->buf + 1, (const u8 __user *) (uintptr_t) addr
		, read_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	pr_debug("len = %d addr = %p val = %x\n", read_len, addr, etspi->buf[2]);

	if (copy_to_user((u8 __user *) (uintptr_t) buf, etspi->buf + 2, read_len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
}

int el7xx_io_burst_read_register(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
	int retval = 0;
	int spi_len = ioc->len + 2;

	if (ioc->len <= 0 || spi_len > bufsiz) {
		pr_err("buffer size fail\n");
		return -ENOMEM;
	}

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_R_S;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), spi_len);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		return retval;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
				ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
}

int el7xx_io_burst_read_register_backward(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
	int retval = 0;
	int spi_len = ioc->len + 2;

	if (ioc->len <= 0 || spi_len > bufsiz) {
		pr_err("buffer size fail\n");
		return -ENOMEM;
	}

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_R_S_BW;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t)ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), spi_len);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		return retval;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
			ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
}

int el7xx_io_write_register(struct el7xx_data *etspi, u8 *buf, struct egis_ioc_transfer *ioc)
{
	int retval = 0;
	int write_len = 2;
	int spi_len = ioc->len + 1;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_W;

	if (copy_from_user(etspi->buf + 1, (const u8 __user *) (uintptr_t) buf,
			write_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	etspi->buf[3] = etspi->buf[2];
	pr_debug("write_len = %d addr = %x data = %x\n",
			write_len, etspi->buf[1], etspi->buf[2]);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	return retval;
}

int el7xx_io_burst_write_register(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
	int retval = 0;
	int spi_len = ioc->len + 1;

	if (ioc->len <= 0 || spi_len + 1 > bufsiz) {
		pr_err("buffer size fail\n");
		return -ENOMEM;
	}

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_W_S;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), spi_len);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		return retval;
	}

	return retval;
}

int el7xx_io_burst_write_register_backward(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
	int retval = 0;
	int spi_len = ioc->len + 1;

	if (ioc->len <= 0 || spi_len + 1 > bufsiz) {
		pr_err("buffer size fail\n");
		return -ENOMEM;
	}

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_W_S_BW;
	if (copy_from_user(etspi->buf + 1,
		(const u8 __user *) (uintptr_t)ioc->tx_buf, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
		ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), spi_len);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		return retval;
	}

	return retval;
}

int el7xx_io_read_efuse(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
	int retval;
	int spi_len = ioc->len + 1;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_EF_R;

	pr_debug("len = %d, xfer.len = %d, buf = %p, rx_buf = %p\n",
			ioc->len, spi_len, etspi->buf, ioc->rx_buf);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf, etspi->buf + 1,
			ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}

	return retval;
}

int el7xx_io_write_efuse(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
	int retval;
	int spi_len = ioc->len + 1;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_EF_W;
	if (copy_from_user((u8 __user *) (uintptr_t) etspi->buf + 1, ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail retval\n");
		return -EFAULT;
	}

	pr_debug("len = %d, xfer.len = %d, buf = %p, tx_buf = %p\n",
			 ioc->len, spi_len, etspi->buf, ioc->tx_buf);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0)
		pr_err("read data error retval = %d\n", retval);

	return retval;
}

int el7xx_io_get_frame(struct el7xx_data *etspi, u8 *fr, u32 size)
{
	int retval;
	int spi_len = size + 1;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_FB_R;

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, spi_len, etspi->buf, fr);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) fr, etspi->buf + 1, size)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}

	return retval;
}

int el7xx_io_write_frame(struct el7xx_data *etspi, u8 *fr, u32 size)
{
	int retval;
	int spi_len = size + 1;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_FB_W;
	if (copy_from_user((u8 __user *)(uintptr_t)etspi->buf + 1, fr, size)) {
		pr_err("buffer copy_from_user fail retval\n");
		return -EFAULT;
	}

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, spi_len, etspi->buf, fr);

	retval = el7xx_spi_sync(etspi, spi_len);
	if (retval < 0)
		pr_err("write data error retval = %d\n", retval);

	return retval;
}

int el7xx_io_transfer_command(struct el7xx_data *etspi, u8 *tx, u8 *rx, u32 size)
{
	int retval = 0;

	memset(etspi->buf, 0, size);
	if (copy_from_user(etspi->buf, (const u8 __user *)(uintptr_t)tx, size)) {
		pr_err("buffer copy_from_user fail. tr(%p), tx(%p)\n", etspi->buf,
		       tx);
		return -EFAULT;
	}

	retval = el7xx_spi_sync(etspi, size);
	if (retval < 0) {
		pr_err("retval = %d\n", retval);
		return retval;
	}

	if (copy_to_user(rx, (const u8 __user *)(uintptr_t)etspi->buf, size)) {
		pr_err("buffer copy_to_user fail. tr(%p) rx(%p)\n", etspi->buf,
		       rx);
		retval = -EFAULT;
	}

	return retval;
}

int el7xx_write_register(struct el7xx_data *etspi, u8 addr, u8 buf)
{
	int retval;
	int spi_len = 3;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_W;
	etspi->buf[1] = addr;
	etspi->buf[2] = buf;
	retval = el7xx_spi_sync(etspi, spi_len);

	if (retval == 0)
		pr_info("address = %x\n", addr);
	else
		pr_err("read data error retval = %d\n", retval);

	return retval;
}

int el7xx_read_register(struct el7xx_data *etspi, u8 addr, u8 *buf)
{
	int retval;
	int spi_len = 4;

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_REG_R;
	etspi->buf[1] = addr;
	retval = el7xx_spi_sync(etspi, spi_len);

	if (retval == 0) {
		*buf = etspi->buf[2];
		pr_info("address = %x result = %x %x\n", addr, etspi->buf[1], etspi->buf[2]);
	} else {
		pr_err("read data error retval = %d\n", retval);
	}

	return retval;
}

int el7xx_init_buffer(struct el7xx_data *etspi)
{
	int retval = 0;

	etspi->buf = kzalloc(bufsiz, GFP_KERNEL);
	if (etspi->buf == NULL) {
		dev_dbg(&etspi->spi->dev, "open/ENOMEM\n");
		return -ENOMEM;
	}

	return retval;
}

int el7xx_free_buffer(struct el7xx_data *etspi)
{
	kfree(etspi->buf);

	return 0;
}
#endif
