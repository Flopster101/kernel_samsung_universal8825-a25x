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

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "et5xx.h"

int et5xx_spi_sync(struct et5xx_data *etspi, int len)
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
		pr_err("error retval = %d\n", retval);

	return retval;
}

int et5xx_io_burst_write_register(struct et5xx_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		pr_err("error retval = %d\n", retval);
		return -ENOMEM;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	etspi->buf[0] = OP_REG_W_C;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), ioc->len + 1);
	retval = et5xx_spi_sync(etspi, ioc->len + 1);

	if (retval < 0)
		pr_err("error retval = %d\n", retval);

	return retval;
#endif
}

int et5xx_io_burst_write_register_backward(struct et5xx_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		pr_err("error retval = %d\n", retval);
		return -ENOMEM;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	etspi->buf[0] = OP_REG_W_C_BW;
	if (copy_from_user(etspi->buf + 1,
		(const u8 __user *) (uintptr_t)ioc->tx_buf, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
		ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), ioc->len + 1);
	retval = et5xx_spi_sync(etspi, ioc->len + 1);

	if (retval < 0)
		pr_err("error retval = %d\n", retval);

	return retval;
#endif
}

int et5xx_io_burst_read_register(struct et5xx_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		pr_err("error retval = %d\n", retval);
		return -ENOMEM;
	}

	memset(etspi->buf, 0, ioc->len + 2);
	etspi->buf[0] = OP_REG_R_C;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), ioc->len + 2);
	retval = et5xx_spi_sync(etspi, ioc->len + 2);

	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		return -ENOMEM;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
				ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
#endif
}

int et5xx_io_burst_read_register_backward(struct et5xx_data *etspi,
		struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		pr_err("error retval = %d\n", retval);
		return -ENOMEM;
	}

	memset(etspi->buf, 0, ioc->len + 2);
	etspi->buf[0] = OP_REG_R_C_BW;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t)ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), ioc->len + 2);
	retval = et5xx_spi_sync(etspi, ioc->len + 2);

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
#endif
}

/* Read io register */
int et5xx_io_read_register(struct et5xx_data *etspi, u8 *addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	int read_len = 1;

	if (copy_from_user(etspi->buf + 1, (const u8 __user *) (uintptr_t) addr
		, read_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}
	etspi->buf[0] = OP_REG_R;
	etspi->buf[2] = 0x00;
	retval = et5xx_spi_sync(etspi, 3);

	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	pr_debug("len = %d addr = %p val = %x\n",
			read_len, addr, etspi->buf[2]);

	if (copy_to_user((u8 __user *) (uintptr_t) buf, etspi->buf + 2, read_len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
#endif
}

/* Write data to register */
int et5xx_io_write_register(struct et5xx_data *etspi, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	int write_len = 2;

	if (copy_from_user(etspi->buf + 1, (const u8 __user *) (uintptr_t) buf,
			write_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	pr_debug("write_len = %d addr = %x data = %x\n",
			write_len, etspi->buf[1], etspi->buf[2]);

	etspi->buf[0] = OP_REG_W;
	retval = et5xx_spi_sync(etspi, 3);

	if (retval < 0)
		pr_err("read data error retval = %d\n", retval);

	return retval;
#endif
}

int et5xx_write_register(struct et5xx_data *etspi, u8 addr, u8 buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;

	mutex_lock(&etspi->buf_lock);
	if (etspi->buf == NULL)
		etspi->buf = kmalloc(bufsiz, GFP_KERNEL);
	if (etspi->buf == NULL) {
		retval = -ENOMEM;
		goto end;
	}

	etspi->buf[0] = OP_REG_W;
	etspi->buf[1] = addr;
	etspi->buf[2] = buf;
	retval = et5xx_spi_sync(etspi, 3);

	if (retval == 0)
		pr_debug("address = %x\n", addr);
	else
		pr_err("read data error retval = %d\n", retval);

	if (etspi->users == 0) {
		kfree(etspi->buf);
		etspi->buf = NULL;
	}
end:
	mutex_unlock(&etspi->buf_lock);
	return retval;
#endif
}

int et5xx_read_register(struct et5xx_data *etspi, u8 addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;

	mutex_lock(&etspi->buf_lock);
	if (etspi->buf == NULL)
		etspi->buf = kmalloc(bufsiz, GFP_KERNEL);
	if (etspi->buf == NULL) {
		retval = -ENOMEM;
		goto end;
	}

	etspi->buf[0] = OP_REG_R;
	etspi->buf[1] = addr;
	etspi->buf[2] = 0x00;
	retval = et5xx_spi_sync(etspi, 3);

	if (retval == 0) {
		*buf = etspi->buf[2];
		pr_debug("address = %x result = %x %x\n", addr, etspi->buf[1], etspi->buf[2]);
	} else {
		pr_err("read data error retval = %d\n", retval);
	}
	if (etspi->users == 0) {
		kfree(etspi->buf);
		etspi->buf = NULL;
	}
end:
	mutex_unlock(&etspi->buf_lock);
	return retval;
#endif
}

int et5xx_io_nvm_read(struct et5xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval, spi_len;
	u8 addr; /* nvm logical address */

	etspi->buf[0] = OP_NVM_RE;
	etspi->buf[1] = 0x00;
	retval = et5xx_spi_sync(etspi, 2);

	if (retval == 0)
		pr_debug("nvm enabled\n");
	else
		pr_err("nvm enable error retval = %d\n", retval);

	usleep_range(10, 50);

	if (copy_from_user(&addr, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, 1)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	etspi->buf[0] = OP_NVM_ON_R;

	pr_debug("logical addr(%x) len(%d)\n", addr, ioc->len);
	if ((addr + ioc->len) > MAX_NVM_LEN)
		return -EINVAL;

	/* transfer to nvm physical address*/
	etspi->buf[1] = ((addr % 2) ? (addr - 1) : addr) / 2;
	/* thansfer to nvm physical length */
	spi_len = ((ioc->len % 2) ? ioc->len + 1 :
			(addr % 2 ? ioc->len + 2 : ioc->len)) + 3;
	if (spi_len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((spi_len) % DIVISION_OF_IMAGE != 0)
			spi_len = spi_len + (DIVISION_OF_IMAGE - (spi_len % DIVISION_OF_IMAGE));
	}

	pr_debug("nvm read addr(%x) len(%d) xfer.rx_buf(%p), etspi->buf(%p)\n",
			etspi->buf[1], spi_len, etspi->buf, etspi->buf);
	retval = et5xx_spi_sync(etspi, spi_len);

	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		return retval;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf, etspi->buf + 3
			, ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
#endif
}

int et5xx_io_nvm_write(struct et5xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval, i, j;
	u8 buf[MAX_NVM_LEN + 1] = {OP_NVM_WE, 0x00};
	u8 addr; /* nvm physical addr */
	int len; /* physical nvm length */

	if (ioc->len > (MAX_NVM_LEN + 1))
		return -EINVAL;

	memcpy(etspi->buf, buf, sizeof(buf));
	retval = et5xx_spi_sync(etspi, 2);

	if (retval == 0)
		pr_debug("nvm enabled\n");
	else
		pr_err("nvm enable error retval = %d\n", retval);

	usleep_range(10, 50);

	pr_debug("buf(%p) tx_buf(%p) len(%d)\n", buf,
			ioc->tx_buf, ioc->len);
	if (copy_from_user(buf, (const u8 __user *) (uintptr_t) ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	if ((buf[0] + (ioc->len - 1)) > MAX_NVM_LEN)
		return -EINVAL;
	if ((buf[0] % 2) || ((ioc->len - 1) % 2)) {
		/* TODO: add non alignment handling */
		pr_err("can't handle address alignment issue. %d %d\n",
				buf[0], ioc->len);
		return -EINVAL;
	}

	len = (ioc->len - 1) / 2;
	pr_debug("nvm write addr(%d) len(%d) xfer.tx_buf(%p), etspi->buf(%p)\n",
			buf[0], len, etspi->buf, etspi->buf);
	for (i = 0, addr = buf[0] / 2; /* thansfer to nvm physical length */
			i < len; i++) {
		etspi->buf[0] = OP_NVM_ON_W;
		etspi->buf[1] = addr++;
		etspi->buf[2] = buf[i * 2 + 1];
		etspi->buf[3] = buf[i * 2 + 2];
		memset(etspi->buf + 4, 1, NVM_WRITE_LENGTH - 4);

		pr_debug("write transaction (%d): %x %x %x %x\n",
				i, etspi->buf[0], etspi->buf[1], etspi->buf[2], etspi->buf[3]);
		retval = et5xx_spi_sync(etspi, NVM_WRITE_LENGTH);

		if (retval < 0) {
			pr_err("error retval = %d\n", retval);
			return retval;
		}
		for (j = 0; j < NVM_WRITE_LENGTH - 4; j++) {
			if (etspi->buf[4 + j] == 0) {
				pr_debug("nvm write ready(%d)\n", j);
				break;
			}
			if (j == NVM_WRITE_LENGTH - 5) {
				pr_err("nvm write fail(timeout)\n");
				return -EIO;
			}
		}
	}

	return retval;
#endif
}

int et5xx_nvm_read(struct et5xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval, spi_len;
	u8 addr; /* nvm logical address */

	etspi->buf[0] = OP_NVM_RE;
	etspi->buf[1] = 0x00;
	retval = et5xx_spi_sync(etspi, 2);

	if (retval == 0)
		pr_debug("nvm enabled\n");
	else
		pr_err("nvm enable error retval = %d\n", retval);

	usleep_range(10, 50);

	addr = ioc->tx_buf[0];

	etspi->buf[0] = OP_NVM_ON_R;

	pr_debug("logical addr(%x) len(%d)\n", addr, ioc->len);
	if ((addr + ioc->len) > MAX_NVM_LEN)
		return -EINVAL;

	/* transfer to nvm physical address*/
	etspi->buf[1] = ((addr % 2) ? (addr - 1) : addr) / 2;
	/* thansfer to nvm physical length */
	spi_len = ((ioc->len % 2) ? ioc->len + 1 :
			(addr % 2 ? ioc->len + 2 : ioc->len)) + 3;
	if (spi_len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((spi_len) % DIVISION_OF_IMAGE != 0)
			spi_len = spi_len + (DIVISION_OF_IMAGE -
					(spi_len % DIVISION_OF_IMAGE));
	}

	pr_debug("nvm read addr(%x) len(%d) xfer.rx_buf(%p), etspi->buf(%p)\n",
			etspi->buf[1], spi_len, etspi->buf, etspi->buf);
	retval = et5xx_spi_sync(etspi, spi_len);

	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		return retval;
	}

	if (memcpy((u8 __user *) (uintptr_t) ioc->rx_buf, etspi->buf + 3,
			ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
#endif
}

int et5xx_io_nvm_off(struct et5xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;

	etspi->buf[0] = OP_NVM_OFF;
	etspi->buf[1] = 0x00;
	retval = et5xx_spi_sync(etspi, 2);

	if (retval == 0)
		pr_debug("nvm disabled\n");
	else
		pr_err("nvm disable error retval = %d\n", retval);

	return retval;
#endif
}
int et5xx_io_vdm_read(struct et5xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	int spi_len = ioc->len + 1;

	if (spi_len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((spi_len) % DIVISION_OF_IMAGE != 0)
			spi_len = spi_len + (DIVISION_OF_IMAGE -
					(spi_len % DIVISION_OF_IMAGE));
	}

	pr_debug("len = %d, xfer.len = %d, buf = %p, rx_buf = %p\n",
			ioc->len, spi_len, etspi->buf, ioc->rx_buf);

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_VDM_R;
	retval = et5xx_spi_sync(etspi, spi_len);

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
#endif
}

int et5xx_io_vdm_write(struct et5xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	int spi_len = ioc->len + 1;

	if (spi_len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((spi_len) % DIVISION_OF_IMAGE != 0)
			spi_len = spi_len + (DIVISION_OF_IMAGE -
					(spi_len % DIVISION_OF_IMAGE));
	}

	memset(etspi->buf, 0, spi_len);
	if (copy_from_user((u8 __user *) (uintptr_t) etspi->buf + 1, ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail retval\n");
		return -EFAULT;
	}

	pr_debug("len = %d, xfer.len = %d, buf = %p, tx_buf = %p\n",
			ioc->len, spi_len, etspi->buf, ioc->tx_buf);

	etspi->buf[0] = OP_VDM_W;
	retval = et5xx_spi_sync(etspi, spi_len);

	if (retval < 0)
		pr_err("read data error retval = %d\n", retval);

	return retval;
#endif
}

int et5xx_io_get_frame(struct et5xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	int spi_len = size + 1;

	if (spi_len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((spi_len) % DIVISION_OF_IMAGE != 0)
			spi_len = spi_len + (DIVISION_OF_IMAGE -
					(spi_len % DIVISION_OF_IMAGE));
	}

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
			size, spi_len, etspi->buf, fr);

	memset(etspi->buf, 0, spi_len);
	etspi->buf[0] = OP_IMG_R;
	retval = et5xx_spi_sync(etspi, spi_len);

	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) fr, etspi->buf + 1, size)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}

	return retval;
#endif
}
